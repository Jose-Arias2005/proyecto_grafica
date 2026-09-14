"""
landmark_detector.py — Detección de landmarks faciales con MediaPipe 1.0+.

Usa la nueva API Tasks (FaceLandmarker) compatible con MediaPipe >= 0.10.x / 1.0+.
El modelo .task se descarga automáticamente a ~/.cache/ar_helmet/ en el primer uso.

Responsabilidades:
  - Inicializar FaceLandmarker y mantener su ciclo de vida.
  - Detectar landmarks en frames RGB.
  - Extraer los 6 puntos canónicos para solvePnP (coincidentes con
    PoseEstimator::canonicalPoints() en C++).
  - Ejecutar cv2.solvePnP y retornar rvec + tvec.
  - Dibujar landmarks y ejes 3D sobre frames para visualización/debug.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple

import urllib.request
import pathlib

import cv2
import mediapipe as mp
import numpy as np
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision as mp_vision
from mediapipe.tasks.python.vision import FaceLandmarker, FaceLandmarkerOptions
from mediapipe.tasks.python.core.base_options import BaseOptions

from camera import CameraIntrinsics

# ─── Modelo MediaPipe FaceLandmarker ─────────────────────────────────────────
# Descarga automática en el primer uso.
_MODEL_URL  = ("https://storage.googleapis.com/mediapipe-models/"
               "face_landmarker/face_landmarker/float16/1/face_landmarker.task")
_CACHE_DIR  = pathlib.Path.home() / ".cache" / "ar_helmet"
_MODEL_PATH = _CACHE_DIR / "face_landmarker.task"


def _ensure_model() -> str:
    """Descarga el modelo .task si no existe en cache. Retorna la ruta local."""
    if not _MODEL_PATH.exists():
        _CACHE_DIR.mkdir(parents=True, exist_ok=True)
        print(f"[LandmarkDetector] Descargando modelo FaceLandmarker -> {_MODEL_PATH}")
        urllib.request.urlretrieve(_MODEL_URL, _MODEL_PATH)
        print("[LandmarkDetector] Modelo descargado.")
    return str(_MODEL_PATH)


# ─── Constantes de landmarks ──────────────────────────────────────────────────

# Índices de MediaPipe Face Mesh 478 para los 6 puntos canónicos.
# DEBEN coincidir con s_canonicalPoints en PoseEstimator.cpp.
LANDMARK_INDICES: Tuple[int, ...] = (
    4,    # Punta de la nariz         (punto origen en el modelo 3D)
    152,  # Mentón
    263,  # Comisura ojo izquierdo (exterior)
    33,   # Comisura ojo derecho  (exterior)
    61,   # Comisura boca izquierda
    291,  # Comisura boca derecha
)

# Puntos 3D canónicos correspondientes (mm), sistema XYZ: derecha/arriba/hacia observador
# Deben ser idénticos a s_canonicalPoints en PoseEstimator.cpp
CANONICAL_3D_POINTS: np.ndarray = np.array([
    [ 0.000,   0.000,   0.000],  # Nariz tip (origen)
    [ 0.000, -63.600, -12.500],  # Menton
    [-43.300,  32.700, -26.000],  # Ojo izquierdo exterior
    [ 43.300,  32.700, -26.000],  # Ojo derecho exterior
    [-28.900, -28.900, -24.100],  # Boca izquierda
    [ 28.900, -28.900, -24.100],  # Boca derecha
], dtype=np.float64)


# ─── Resultado de detección ───────────────────────────────────────────────────

@dataclass
class DetectionResult:
    """Resultado de un frame de detección de landmarks.

    Attributes:
        landmarks_2d: Array (6, 2) float32 con los puntos 2D en píxeles.
        all_landmarks: Array (478, 2) float32 con todos los landmarks (para debug).
        rvec: Vector de Rodrigues (3,) float64 de solvePnP.
        tvec: Vector de traslación (3,) float64 de solvePnP, en mm.
        success: True si la detección y PnP fueron exitosos.
        confidence: Score de confianza de la detección [0, 1].
    """
    landmarks_2d:   Optional[np.ndarray] = None  # (6, 2) float32
    all_landmarks:  Optional[np.ndarray] = None  # (478, 2) float32
    rvec:           Optional[np.ndarray] = None  # (3,) float64
    tvec:           Optional[np.ndarray] = None  # (3,) float64
    success:        bool  = False
    confidence:     float = 0.0


# ─── Detector de landmarks ────────────────────────────────────────────────────

class LandmarkDetector:
    """Detector de landmarks faciales con MediaPipe FaceLandmarker (Tasks API 1.0+) y solvePnP.

    Extrae los 6 puntos canonicos (nariz, menton, ojos, boca) para estimar
    la pose 6-DoF de la cabeza.

    Uso con context manager:
        with LandmarkDetector(intrinsics) as detector:
            result = detector.detect(rgb_frame)
            if result.success:
                rvec, tvec = result.rvec, result.tvec

    Nota: MediaPipe espera frames RGB (no BGR).
    """

    def __init__(
        self,
        intrinsics:       CameraIntrinsics,
        max_faces:        int   = 1,
        refine_landmarks: bool  = True,
        min_detection:    float = 0.5,
        min_tracking:     float = 0.5,
    ) -> None:
        """
        Args:
            intrinsics:       Parametros intrinsecos de la camara.
            max_faces:        Numero maximo de caras a detectar.
            refine_landmarks: Activar landmarks de iris (mayor precision).
            min_detection:    Confianza minima de deteccion inicial [0,1].
            min_tracking:     Confianza minima de tracking [0,1].
        """
        self._intrinsics     = intrinsics
        self._max_faces      = max_faces
        self._refine         = refine_landmarks
        self._min_detection  = min_detection
        self._min_tracking   = min_tracking
        self._landmarker: Optional[FaceLandmarker] = None

    # ── Context Manager ───────────────────────────────────────────────────────

    def __enter__(self) -> "LandmarkDetector":
        self.open()
        return self

    def __exit__(self, *_) -> None:
        self.close()

    def open(self) -> None:
        """Inicializa el modelo FaceLandmarker de MediaPipe Tasks API."""
        model_path = _ensure_model()
        options = FaceLandmarkerOptions(
            base_options          = BaseOptions(model_asset_path=model_path),
            running_mode          = mp_vision.RunningMode.IMAGE,
            num_faces             = self._max_faces,
            min_face_detection_confidence = self._min_detection,
            min_face_presence_confidence  = self._min_detection,
            min_tracking_confidence       = self._min_tracking,
            output_face_blendshapes       = False,
            output_facial_transformation_matrixes = False,
        )
        self._landmarker = FaceLandmarker.create_from_options(options)
        print("[LandmarkDetector] FaceLandmarker (Tasks API) inicializado "
              f"(max_faces={self._max_faces})")

    def close(self) -> None:
        """Libera el modelo de MediaPipe."""
        if self._landmarker is not None:
            self._landmarker.close()
            self._landmarker = None

    # ── Detección principal ───────────────────────────────────────────────────

    def detect(self, rgb_frame: np.ndarray) -> DetectionResult:
        """Detecta landmarks y ejecuta solvePnP en un frame RGB.

        Args:
            rgb_frame: Frame RGB, shape (H, W, 3), dtype=uint8.

        Returns:
            DetectionResult con rvec/tvec si la deteccion fue exitosa.
        """
        result = DetectionResult()
        if self._landmarker is None:
            return result

        h, w = rgb_frame.shape[:2]

        # ── MediaPipe FaceLandmarker (Tasks API) ──────────────────────────────
        mp_image   = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)
        mp_result  = self._landmarker.detect(mp_image)

        if not mp_result.face_landmarks:
            return result

        # Tomar la primera cara detectada
        face_lm = mp_result.face_landmarks[0]  # Lista de NormalizedLandmark

        # Todos los landmarks (478 puntos) en pixeles
        all_lm = np.array(
            [[lm.x * w, lm.y * h] for lm in face_lm],
            dtype=np.float32
        )
        result.all_landmarks = all_lm
        result.confidence    = 1.0

        # ── Extraer los 6 puntos canonicos ─────────────────────────────────────
        points_2d = all_lm[list(LANDMARK_INDICES)]  # Shape (6, 2)
        result.landmarks_2d = points_2d

        # ── solvePnP ──────────────────────────────────────────────────────────
        success, rvec, tvec = cv2.solvePnP(
            CANONICAL_3D_POINTS,
            points_2d.astype(np.float64),
            self._intrinsics.matrix,
            self._intrinsics.dist_coeffs,
            flags=cv2.SOLVEPNP_ITERATIVE,
        )

        if not success:
            return result

        result.rvec    = rvec.flatten().astype(np.float32)
        result.tvec    = tvec.flatten().astype(np.float32)
        result.success = True
        return result

    # ── Visualización / Debug ─────────────────────────────────────────────────

    def draw_landmarks(
        self,
        bgr_frame:  np.ndarray,
        result:     DetectionResult,
        draw_mesh:  bool = False,
    ) -> np.ndarray:
        """Dibuja los 6 puntos canónicos sobre el frame BGR.

        Args:
            bgr_frame:  Frame BGR a anotar (se copia internamente).
            result:     DetectionResult de detect().
            draw_mesh:  Si True, también dibuja el mesh completo de 478 puntos.

        Returns:
            Frame BGR anotado.
        """
        out = bgr_frame.copy()
        if not result.success or result.landmarks_2d is None:
            return out

        # Puntos canónicos (círculos verdes)
        for i, (x, y) in enumerate(result.landmarks_2d):
            cv2.circle(out, (int(x), int(y)), 4, (0, 255, 0), -1)
            cv2.putText(out, str(LANDMARK_INDICES[i]),
                        (int(x) + 5, int(y) - 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.35, (0, 255, 0), 1)

        # Mesh completo (opcional, puntos blancos pequeños)
        if draw_mesh and result.all_landmarks is not None:
            for (x, y) in result.all_landmarks:
                cv2.circle(out, (int(x), int(y)), 1, (200, 200, 200), -1)

        return out

    def draw_axes(
        self,
        bgr_frame: np.ndarray,
        result:    DetectionResult,
        axis_len:  float = 30.0,
    ) -> np.ndarray:
        """Dibuja los ejes 3D de la pose sobre el frame BGR.

        Requiere que result.success sea True.

        Args:
            bgr_frame: Frame BGR a anotar.
            result:    DetectionResult con rvec/tvec válidos.
            axis_len:  Longitud de los ejes en mm.

        Returns:
            Frame BGR con los ejes XYZ (rojo/verde/azul) proyectados.
        """
        out = bgr_frame.copy()
        if not result.success:
            return out

        try:
            cv2.drawFrameAxes(
                out,
                self._intrinsics.matrix,
                self._intrinsics.dist_coeffs,
                result.rvec.astype(np.float64),
                result.tvec.astype(np.float64),
                axis_len,
                thickness=2,
            )
        except cv2.error as e:
            print(f"[LandmarkDetector] Error en drawFrameAxes: {e}")

        return out

    # ── Propiedades ───────────────────────────────────────────────────────────

    @property
    def intrinsics(self) -> CameraIntrinsics:
        return self._intrinsics

    @intrinsics.setter
    def intrinsics(self, value: CameraIntrinsics) -> None:
        self._intrinsics = value

    @property
    def canonical_3d_points(self) -> np.ndarray:
        """Puntos 3D del modelo canónico de cara (mm)."""
        return CANONICAL_3D_POINTS.copy()

    @property
    def landmark_indices(self) -> Tuple[int, ...]:
        """Índices de MediaPipe de los 6 puntos canónicos."""
        return LANDMARK_INDICES
