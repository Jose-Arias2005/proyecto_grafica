"""
main.py — Orquestador principal de AR-Helmet.

Pipeline por frame:
  Camera.grab_frame()
    → LandmarkDetector.detect(rgb)   [MediaPipe + solvePnP]
    → ar_helmet_core.PoseEstimator.build_model_view(rvec, tvec)
    → ar_helmet_core.Renderer.upload_frame(bgr)
    → ar_helmet_core.Renderer.set_lighting(light_estimate)
    → ar_helmet_core.Renderer.render(model_view)
    → ar_helmet_core.Renderer.get_frame()  → bgr_composed
    → MainWindow.show_frame(bgr_composed)

RESTRICCIÓN: Esta aplicación no implementa la lógica de LightEstimator.
             Usa LightEstimate.default_estimate() hasta que el equipo humano
             complete LightEstimator.cpp.
"""
from __future__ import annotations

import sys
import time
import argparse
from pathlib import Path
from typing import Optional

import cv2
import numpy as np
from PyQt6.QtCore    import QTimer, Qt
from PyQt6.QtWidgets import QApplication

# ── Módulos propios ───────────────────────────────────────────────────────────
# Agregar app_ui al path si se ejecuta desde la raíz
_ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(_ROOT))
sys.path.insert(0, str(Path(__file__).parent))

from camera           import Camera, CameraIntrinsics
from landmark_detector import LandmarkDetector
from gui               import MainWindow

# ── Módulo nativo C++ (compilado) ─────────────────────────────────────────────
try:
    import ar_helmet_core as core
    CORE_AVAILABLE = True
except ImportError:
    CORE_AVAILABLE = False
    print("[main] ADVERTENCIA: ar_helmet_core no encontrado. "
          "Ejecutar en modo solo-Python (sin render 3D).")


# ─── Orquestador ──────────────────────────────────────────────────────────────

class ARHelmetApp:
    """Orquestador principal del pipeline AR-Helmet."""

    # Rutas relativas a la raíz del proyecto
    SHADER_DIR    = "assets/shaders"
    HELMET_OBJ    = "assets/models/helmet.obj"
    OCCLUDER_OBJ  = "assets/models/face_occluder.obj"

    def __init__(self, args: argparse.Namespace) -> None:
        self._args           = args
        self._camera:        Optional[Camera]            = None
        self._detector:      Optional[LandmarkDetector]  = None
        self._renderer:      Optional["core.Renderer"]   = None       # type: ignore
        self._pose_estimator:Optional["core.PoseEstimator"] = None   # type: ignore
        self._light_est:     Optional["core.LightEstimator"] = None  # type: ignore

        # Estado de UI
        self._show_debug   = True
        self._show_helmet  = True
        self._show_occluder= True
        self._show_mesh    = False

        # Métricas de performance
        self._fps_counter  = 0
        self._fps_ts       = time.time()
        self._fps          = 0.0
        self._detect_ms    = 0.0
        self._render_ms    = 0.0

        # Qt
        self._qt_app       = QApplication.instance() or QApplication(sys.argv)
        self._window       = MainWindow()
        self._timer        = QTimer()

    def setup(self) -> None:
        """Inicializa todos los subsistemas."""
        # ── Cámara ─────────────────────────────────────────────────────────
        self._camera = Camera(
            device_id = self._args.camera,
            width     = self._args.width,
            height    = self._args.height,
            fps       = self._args.fps,
        )
        self._camera.open()
        intrinsics = self._camera.intrinsics

        # ── Detector de landmarks ──────────────────────────────────────────
        self._detector = LandmarkDetector(intrinsics)
        self._detector.open()

        # ── Core C++ (si está compilado) ───────────────────────────────────
        if CORE_AVAILABLE and intrinsics:
            self._pose_estimator = core.PoseEstimator(
                intrinsics.fx, intrinsics.fy,
                intrinsics.cx, intrinsics.cy,
            )
            self._light_est = core.LightEstimator()

            w, h = self._camera.frame_size
            self._renderer = core.Renderer(w, h)

            root = str(_ROOT)
            self._renderer.init(
                f"{root}/{self.SHADER_DIR}",
                f"{root}/{self.HELMET_OBJ}",
                f"{root}/{self.OCCLUDER_OBJ}",
            )
            self._renderer.set_projection(
                intrinsics.fx, intrinsics.fy,
                intrinsics.cx, intrinsics.cy,
            )

        # ── Conectar señales UI ────────────────────────────────────────────
        ctrl = self._window.controls
        ctrl.debug_toggled.connect(   lambda v: setattr(self, "_show_debug",    v))
        ctrl.helmet_toggled.connect(  lambda v: setattr(self, "_show_helmet",   v))
        ctrl.occluder_toggled.connect(lambda v: setattr(self, "_show_occluder", v))
        ctrl.mesh_toggled.connect(    lambda v: setattr(self, "_show_mesh",     v))
        ctrl.camera_requested.connect(self._restart_camera)

        print(f"[main] AR-Helmet inicializado ({'C++ core OK' if CORE_AVAILABLE else 'modo Python'})")

    def run(self) -> int:
        """Inicia el loop principal y retorna el código de salida."""
        self._window.show()
        self._timer.timeout.connect(self._process_frame)
        self._timer.start(0)  # Interval 0 = máxima velocidad (limitado por QTimer)
        return self._qt_app.exec()

    def teardown(self) -> None:
        """Libera todos los recursos."""
        self._timer.stop()
        if self._detector:
            self._detector.close()
        if self._camera:
            self._camera.release()

    # ── Loop principal ────────────────────────────────────────────────────────

    def _process_frame(self) -> None:
        """Ejecuta un ciclo completo del pipeline AR."""
        if not self._camera or not self._camera.is_open:
            return

        bgr = self._camera.grab_frame()
        if bgr is None:
            return

        # ── Detección de landmarks + solvePnP ─────────────────────────────
        t0 = time.perf_counter()
        rgb    = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
        result = self._detector.detect(rgb) if self._detector else None
        self._detect_ms = (time.perf_counter() - t0) * 1000.0

        # ── Render 3D (si core compilado y pose detectada) ─────────────────
        t1 = time.perf_counter()
        output_frame = bgr.copy()

        if CORE_AVAILABLE and self._renderer and result and result.success:
            # Estimar iluminación (placeholder hasta implementación humana)
            light = self._light_est.estimate(bgr) if self._light_est \
                    else core.LightEstimate.default_estimate()

            # Construir Model-View
            mv = self._pose_estimator.build_model_view(result.rvec, result.tvec)

            # Pipeline GL
            self._renderer.upload_frame(bgr)
            self._renderer.set_lighting(light)
            self._renderer.render(mv)
            output_frame = self._renderer.get_frame()

        self._render_ms = (time.perf_counter() - t1) * 1000.0

        # ── Overlays de debug ──────────────────────────────────────────────
        if self._show_debug and self._detector and result:
            output_frame = self._detector.draw_landmarks(
                output_frame, result, draw_mesh=self._show_mesh)
            if result.success:
                output_frame = self._detector.draw_axes(output_frame, result)

        # ── FPS counter ───────────────────────────────────────────────────
        self._fps_counter += 1
        now = time.time()
        if now - self._fps_ts >= 1.0:
            self._fps        = self._fps_counter / (now - self._fps_ts)
            self._fps_counter = 0
            self._fps_ts     = now

        # ── OSD (On-Screen Display) ────────────────────────────────────────
        cv2.putText(output_frame,
                    f"FPS: {self._fps:.1f}",
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

        # ── Actualizar UI ─────────────────────────────────────────────────
        self._window.show_frame(output_frame)
        self._window.update_metrics(
            self._fps, self._detect_ms, self._render_ms,
            result.success if result else False,
        )

    def _restart_camera(self) -> None:
        if self._camera:
            self._camera.release()
            self._camera.open()


# ─── Entry point ──────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="AR-Helmet — CS4016")
    p.add_argument("--camera", type=int,  default=0,    help="ID del dispositivo de cámara")
    p.add_argument("--width",  type=int,  default=1280, help="Ancho de captura (px)")
    p.add_argument("--height", type=int,  default=720,  help="Alto de captura (px)")
    p.add_argument("--fps",    type=int,  default=60,   help="FPS objetivo")
    return p.parse_args()


def main() -> None:
    args   = parse_args()
    app_hl = ARHelmetApp(args)
    try:
        app_hl.setup()
        sys.exit(app_hl.run())
    finally:
        app_hl.teardown()


if __name__ == "__main__":
    main()
