"""
test_pose.py — Script de prueba autónomo para la Semana 5.
================================================================
Valida el pipeline Python puro (sin C++ compilado):
  1. Apertura de webcam con OpenCV.
  2. Detección de landmarks con MediaPipe Face Mesh.
  3. Estimación de pose 6-DoF con cv2.solvePnP (ITERATIVE).
  4. Dibujo de ejes 3D con cv2.drawFrameAxes.
  5. Display de FPS en tiempo real.

NO requiere compilar core_engine.  Solo necesita:
  pip install opencv-python mediapipe numpy

Ejecución:
  python tests/test_pose.py
  python tests/test_pose.py --camera 1 --width 1280 --height 720
  python tests/test_pose.py --no-gui          # Modo headless (guarda video)
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import cv2
import numpy as np

# ── Resolver imports desde raíz del proyecto ──────────────────────────────────
_ROOT    = Path(__file__).parent.parent
_APP_UI  = _ROOT / "app_ui"
sys.path.insert(0, str(_APP_UI))
sys.path.insert(0, str(_ROOT))

from camera            import Camera, CameraIntrinsics
from landmark_detector import (
    LandmarkDetector,
    LANDMARK_INDICES,
    CANONICAL_3D_POINTS,
    DetectionResult,
)


# ─── Configuración ─────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="AR-Helmet — Test Semana 5: Captura + Landmarks + PnP + Ejes 3D",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--camera",   type=int,   default=0,    help="ID de dispositivo de cámara")
    p.add_argument("--width",    type=int,   default=1280, help="Ancho de captura (px)")
    p.add_argument("--height",   type=int,   default=720,  help="Alto de captura (px)")
    p.add_argument("--fps",      type=int,   default=60,   help="FPS objetivo de captura")
    p.add_argument("--axis-len", type=float, default=40.0, help="Longitud de ejes 3D (mm)")
    p.add_argument("--no-gui",   action="store_true",      help="Modo headless: guarda video")
    p.add_argument("--output",   type=str,   default="test_pose_output.mp4",
                   help="Archivo de salida en modo --no-gui")
    p.add_argument("--max-frames", type=int, default=0,
                   help="Límite de frames (0 = sin límite)")
    return p.parse_args()


# ─── OSD (On-Screen Display) ──────────────────────────────────────────────────

def draw_osd(
    frame:      np.ndarray,
    fps:        float,
    detect_ms:  float,
    pose_ok:    bool,
    tvec:       np.ndarray | None,
) -> np.ndarray:
    """Dibuja información de diagnóstico sobre el frame."""
    out = frame.copy()
    h, w = out.shape[:2]

    # Fondo semitransparente para el texto
    overlay = out.copy()
    cv2.rectangle(overlay, (0, 0), (360, 110), (0, 0, 0), -1)
    cv2.addWeighted(overlay, 0.5, out, 0.5, 0, out)

    font      = cv2.FONT_HERSHEY_SIMPLEX
    color_ok  = (80, 230, 80)
    color_err = (80, 80, 230)
    color_inf = (200, 200, 200)

    cv2.putText(out, f"FPS: {fps:5.1f}",
                (10, 28),  font, 0.75, color_ok,  2)
    cv2.putText(out, f"Deteccion: {detect_ms:5.1f} ms",
                (10, 54),  font, 0.65, color_inf, 1)

    pose_str   = "POSE: OK" if pose_ok else "POSE: sin deteccion"
    pose_color = color_ok if pose_ok else color_err
    cv2.putText(out, pose_str, (10, 80), font, 0.65, pose_color, 2)

    if pose_ok and tvec is not None:
        dist_mm = float(np.linalg.norm(tvec))
        cv2.putText(out, f"Dist: {dist_mm:.0f} mm",
                    (10, 104), font, 0.60, color_inf, 1)

    # Instrucciones (esquina inferior)
    cv2.putText(out, "Q: salir   D: debug mesh   S: captura",
                (10, h - 10), font, 0.45, (150, 150, 150), 1)

    return out


# ─── Loop principal ────────────────────────────────────────────────────────────

def run_test(args: argparse.Namespace) -> int:
    """Ejecuta el loop de prueba. Retorna 0 si OK, 1 si error."""

    print("=" * 60)
    print("AR-Helmet — Test Semana 5")
    print(f"  Cámara    : {args.camera}")
    print(f"  Resolución: {args.width}×{args.height} @ {args.fps} FPS")
    print(f"  Modo      : {'Headless (guarda video)' if args.no_gui else 'Ventana GUI'}")
    print("  Controles : Q=salir  D=debug mesh  S=guardar captura")
    print("=" * 60)

    # ── Video writer (modo headless) ──────────────────────────────────────
    writer: cv2.VideoWriter | None = None
    if args.no_gui:
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        writer = cv2.VideoWriter(
            args.output, fourcc, float(args.fps),
            (args.width, args.height)
        )
        print(f"[test_pose] Grabando en: {args.output}")

    show_mesh  = False
    frame_idx  = 0
    fps        = 0.0
    detect_ms  = 0.0
    fps_frames = 0
    fps_ts     = time.time()
    cap_idx    = 0

    with Camera(args.camera, args.width, args.height, args.fps) as cam:
        intrinsics = cam.intrinsics
        if intrinsics is None:
            print("[test_pose] ERROR: No se pudo obtener intrínsecos de la cámara.")
            return 1

        print(f"[test_pose] Intrínsecos: {intrinsics}")

        with LandmarkDetector(intrinsics) as detector:
            print("[test_pose] Iniciando captura... (presiona Q para salir)\n")

            while True:
                # ── Captura ───────────────────────────────────────────────
                bgr = cam.grab_frame()
                if bgr is None:
                    print("[test_pose] ADVERTENCIA: frame vacío.")
                    continue

                # ── Detección + solvePnP ──────────────────────────────────
                t0   = time.perf_counter()
                rgb  = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
                result = detector.detect(rgb)
                detect_ms = (time.perf_counter() - t0) * 1000.0

                # ── Visualización ─────────────────────────────────────────
                display = bgr.copy()

                # Landmarks + ejes 3D
                display = detector.draw_landmarks(
                    display, result, draw_mesh=show_mesh)
                if result.success:
                    display = detector.draw_axes(
                        display, result, axis_len=args.axis_len)

                # OSD
                display = draw_osd(
                    display, fps, detect_ms,
                    result.success,
                    result.tvec if result.success else None,
                )

                # ── Output ────────────────────────────────────────────────
                if args.no_gui and writer:
                    writer.write(display)
                else:
                    cv2.imshow("AR-Helmet | Test Semana 5", display)

                # ── FPS ───────────────────────────────────────────────────
                fps_frames += 1
                now = time.time()
                if now - fps_ts >= 1.0:
                    fps       = fps_frames / (now - fps_ts)
                    fps_frames = 0
                    fps_ts    = now
                    pose_str  = f"POSE OK  tvec={result.tvec}" if result.success \
                                else "sin pose"
                    print(f"[test_pose] FPS={fps:.1f}  detección={detect_ms:.1f}ms  {pose_str}")

                # ── Teclado ───────────────────────────────────────────────
                key = cv2.waitKey(1) & 0xFF
                if key == ord("q") or key == 27:          # Q / ESC: salir
                    break
                elif key == ord("d"):                      # D: toggle debug mesh
                    show_mesh = not show_mesh
                    print(f"[test_pose] Face mesh: {'ON' if show_mesh else 'OFF'}")
                elif key == ord("s"):                      # S: guardar captura
                    fname = f"capture_{cap_idx:04d}.png"
                    cv2.imwrite(fname, display)
                    print(f"[test_pose] Captura guardada: {fname}")
                    cap_idx += 1

                # ── Límite de frames ──────────────────────────────────────
                frame_idx += 1
                if args.max_frames > 0 and frame_idx >= args.max_frames:
                    print(f"[test_pose] Alcanzado límite de {args.max_frames} frames.")
                    break

    cv2.destroyAllWindows()
    if writer:
        writer.release()
        print(f"[test_pose] Video guardado: {args.output}")

    print(f"\n[test_pose] Finalizado. {frame_idx} frames procesados.")
    return 0


# ─── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    sys.exit(run_test(parse_args()))
