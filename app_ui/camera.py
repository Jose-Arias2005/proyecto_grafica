"""
camera.py — Módulo de captura de cámara con OpenCV.

Responsabilidades:
  - Abrir y configurar el dispositivo de captura (resolución, FPS, buffer).
  - Proveer frames BGR y RGB en formato NumPy contiguo.
  - Estimar y exponer parámetros intrínsecos de la cámara (modelo pinhole).
  - Gestión de ciclo de vida RAII via context manager.
"""
from __future__ import annotations

import cv2
import numpy as np
from dataclasses import dataclass, field
from typing import Optional, Tuple


# ─── Intrínsecos de cámara ────────────────────────────────────────────────────

@dataclass
class CameraIntrinsics:
    """Parámetros intrínsecos del modelo pinhole (focal lengths + punto principal).

    Attributes:
        fx: Longitud focal horizontal en píxeles.
        fy: Longitud focal vertical en píxeles.
        cx: Componente X del punto principal (px).
        cy: Componente Y del punto principal (px).
        dist_coeffs: Coeficientes de distorsión (k1,k2,p1,p2,k3).
    """
    fx: float
    fy: float
    cx: float
    cy: float
    dist_coeffs: np.ndarray = field(
        default_factory=lambda: np.zeros(5, dtype=np.float64))

    # ── Factory methods ───────────────────────────────────────────────────────

    @classmethod
    def from_frame_size(cls, width: int, height: int) -> "CameraIntrinsics":
        """Estimación heurística para cámara sin calibración formal.

        Usa como focal length el lado mayor del frame (aproximación conservadora
        válida para cámaras web típicas con FoV ~60-70°).
        """
        focal = float(max(width, height))
        return cls(
            fx=focal,
            fy=focal,
            cx=width  / 2.0,
            cy=height / 2.0,
        )

    @classmethod
    def from_calibration_file(cls, path: str) -> "CameraIntrinsics":
        """Carga intrínsecos desde archivo YAML/XML de calibración de OpenCV."""
        fs = cv2.FileStorage(path, cv2.FILE_STORAGE_READ)
        K  = fs.getNode("camera_matrix").mat()
        d  = fs.getNode("dist_coeffs").mat()
        fs.release()
        if K is None:
            raise IOError(f"No se encontró 'camera_matrix' en: {path}")
        return cls(
            fx=float(K[0, 0]),
            fy=float(K[1, 1]),
            cx=float(K[0, 2]),
            cy=float(K[1, 2]),
            dist_coeffs=d.flatten().astype(np.float64) if d is not None
                        else np.zeros(5, dtype=np.float64),
        )

    # ── Propiedades ───────────────────────────────────────────────────────────

    @property
    def matrix(self) -> np.ndarray:
        """Matriz de cámara K (3×3) en formato estándar de OpenCV."""
        return np.array([
            [self.fx,   0.0, self.cx],
            [  0.0,  self.fy, self.cy],
            [  0.0,    0.0,   1.0  ],
        ], dtype=np.float64)

    def __repr__(self) -> str:
        return (f"CameraIntrinsics(fx={self.fx:.1f}, fy={self.fy:.1f}, "
                f"cx={self.cx:.1f}, cy={self.cy:.1f})")


# ─── Cámara ───────────────────────────────────────────────────────────────────

class Camera:
    """Wrapper de OpenCV VideoCapture con gestión de ciclo de vida RAII.

    Optimizado para latencia mínima (buffer=1) y captura a alta tasa.

    Uso con context manager:
        with Camera(device_id=0, width=1280, height=720) as cam:
            frame = cam.grab_frame()   # BGR np.ndarray
            rgb   = cam.grab_rgb_frame()

    Uso manual:
        cam = Camera()
        cam.open()
        frame = cam.grab_frame()
        cam.release()
    """

    def __init__(
        self,
        device_id:  int = 0,
        width:      int = 1280,
        height:     int = 720,
        fps:        int = 60,
        intrinsics: Optional[CameraIntrinsics] = None,
    ) -> None:
        self._device_id      = device_id
        self._target_width   = width
        self._target_height  = height
        self._target_fps     = fps
        self._cap:           Optional[cv2.VideoCapture] = None
        self._intrinsics:    Optional[CameraIntrinsics] = intrinsics
        self._frame_count:   int = 0
        self._actual_width:  int = width
        self._actual_height: int = height

    # ── Context Manager ───────────────────────────────────────────────────────

    def __enter__(self) -> "Camera":
        self.open()
        return self

    def __exit__(self, *_) -> None:
        self.release()

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def open(self) -> None:
        """Abre el dispositivo de cámara y configura resolución/FPS.

        Raises:
            RuntimeError: Si no se puede abrir el dispositivo.
        """
        self._cap = cv2.VideoCapture(self._device_id)
        if not self._cap.isOpened():
            raise RuntimeError(
                f"[Camera] No se pudo abrir el dispositivo {self._device_id}. "
                "Verifica que la cámara no esté en uso por otra aplicación.")

        # Configurar resolución y FPS deseados
        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH,  self._target_width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self._target_height)
        self._cap.set(cv2.CAP_PROP_FPS,          self._target_fps)

        # Buffer mínimo para reducir latencia (captura el frame más reciente)
        self._cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        # Leer resolución real configurada por el driver
        self._actual_width  = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self._actual_height = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        actual_fps          = self._cap.get(cv2.CAP_PROP_FPS)

        # Generar intrínsecos estimados si no se proveyeron
        if self._intrinsics is None:
            self._intrinsics = CameraIntrinsics.from_frame_size(
                self._actual_width, self._actual_height)

        print(f"[Camera] Dispositivo {self._device_id} abierto: "
              f"{self._actual_width}×{self._actual_height} @ {actual_fps:.0f} FPS")
        print(f"[Camera] Intrínsecos: {self._intrinsics}")

    def release(self) -> None:
        """Libera el dispositivo de cámara."""
        if self._cap is not None and self._cap.isOpened():
            self._cap.release()
            self._cap = None
            print(f"[Camera] Dispositivo {self._device_id} liberado.")

    # ── Captura ───────────────────────────────────────────────────────────────

    def grab_frame(self) -> Optional[np.ndarray]:
        """Captura un frame BGR (H×W×3, uint8) de la cámara.

        Returns:
            np.ndarray BGR contiguo, o None si la captura falló.
        """
        if self._cap is None or not self._cap.isOpened():
            return None

        ret, frame = self._cap.read()
        if not ret or frame is None:
            return None

        self._frame_count += 1
        # Asegurar que el array es contiguo en C-order (requerido por pybind11)
        return np.ascontiguousarray(frame)

    def grab_rgb_frame(self) -> Optional[np.ndarray]:
        """Captura un frame y lo convierte a RGB (H×W×3, uint8).

        Útil para MediaPipe, que espera RGB.
        """
        frame = self.grab_frame()
        if frame is None:
            return None
        return cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    def grab_undistorted_frame(self) -> Optional[np.ndarray]:
        """Captura y corrige distorsión de lente (si dist_coeffs != 0)."""
        frame = self.grab_frame()
        if frame is None:
            return None
        if self._intrinsics is None:
            return frame
        d = self._intrinsics.dist_coeffs
        if np.allclose(d, 0):
            return frame
        return cv2.undistort(frame, self._intrinsics.matrix, d)

    # ── Propiedades ───────────────────────────────────────────────────────────

    @property
    def intrinsics(self) -> Optional[CameraIntrinsics]:
        """Parámetros intrínsecos de la cámara."""
        return self._intrinsics

    @intrinsics.setter
    def intrinsics(self, value: CameraIntrinsics) -> None:
        self._intrinsics = value

    @property
    def frame_size(self) -> Tuple[int, int]:
        """Tupla (width, height) de la resolución real de captura."""
        return (self._actual_width, self._actual_height)

    @property
    def frame_count(self) -> int:
        """Número total de frames capturados desde open()."""
        return self._frame_count

    @property
    def is_open(self) -> bool:
        """True si el dispositivo está abierto y funcionando."""
        return self._cap is not None and self._cap.isOpened()

    @property
    def device_id(self) -> int:
        return self._device_id

    def __repr__(self) -> str:
        status = "abierta" if self.is_open else "cerrada"
        return (f"Camera(device={self._device_id}, "
                f"{self._actual_width}×{self._actual_height}, {status})")
