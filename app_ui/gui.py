"""
gui.py — Interfaz gráfica PyQt6 para AR-Helmet.

Responsabilidades:
  - Ventana principal con preview de cámara en tiempo real.
  - Panel lateral de controles (activar/desactivar casco, modos de debug).
  - Display de métricas de performance (FPS, latencia de detección).
  - Señales Qt para comunicación desacoplada con el orquestador main.py.
"""
from __future__ import annotations

from typing import Optional

import cv2
import numpy as np
from PyQt6.QtCore    import Qt, QTimer, pyqtSignal, QThread, pyqtSlot
from PyQt6.QtGui     import QImage, QPixmap, QFont, QKeySequence, QShortcut
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QVBoxLayout,
    QHBoxLayout, QPushButton, QCheckBox, QSlider, QGroupBox,
    QStatusBar, QSplitter, QFrame,
)


# ─── Widget de preview de cámara ─────────────────────────────────────────────

class CameraPreview(QLabel):
    """QLabel optimizado para mostrar frames de cámara en tiempo real."""

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setMinimumSize(640, 480)
        self.setStyleSheet("background-color: #111; border: 2px solid #333;")
        self.setText("Sin señal de cámara")
        self.setFont(QFont("Segoe UI", 14))

    def update_frame(self, bgr_frame: np.ndarray) -> None:
        """Convierte un frame BGR NumPy a QPixmap y lo muestra.

        Args:
            bgr_frame: np.ndarray (H, W, 3) uint8 BGR.
        """
        h, w, ch = bgr_frame.shape
        # Convertir BGR → RGB para Qt
        rgb = cv2.cvtColor(bgr_frame, cv2.COLOR_BGR2RGB)
        q_img = QImage(rgb.data, w, h, w * ch, QImage.Format.Format_RGB888)
        pixmap = QPixmap.fromImage(q_img)
        # Escalar preservando aspecto ratio
        self.setPixmap(
            pixmap.scaled(self.size(),
                          Qt.AspectRatioMode.KeepAspectRatio,
                          Qt.TransformationMode.SmoothTransformation)
        )


# ─── Panel de controles ───────────────────────────────────────────────────────

class ControlPanel(QWidget):
    """Panel lateral con controles e indicadores de estado."""

    # Señales emitidas hacia el orquestador
    helmet_toggled     = pyqtSignal(bool)   # Activar/desactivar casco 3D
    debug_toggled      = pyqtSignal(bool)   # Modo debug (landmarks + ejes)
    mesh_toggled       = pyqtSignal(bool)   # Mostrar mesh completo
    occluder_toggled   = pyqtSignal(bool)   # Activar/desactivar occluder
    camera_requested   = pyqtSignal()       # Solicitar reinicio de cámara

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setFixedWidth(240)
        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(12)
        layout.setContentsMargins(12, 12, 12, 12)

        # ── Título ─────────────────────────────────────────────────────────
        title = QLabel("AR-Helmet")
        title.setFont(QFont("Segoe UI", 16, QFont.Weight.Bold))
        title.setStyleSheet("color: #4fc3f7;")
        layout.addWidget(title)

        subtitle = QLabel("CS4016 — Computación Gráfica")
        subtitle.setFont(QFont("Segoe UI", 9))
        subtitle.setStyleSheet("color: #888;")
        layout.addWidget(subtitle)

        separator = QFrame()
        separator.setFrameShape(QFrame.Shape.HLine)
        separator.setStyleSheet("color: #444;")
        layout.addWidget(separator)

        # ── Grupo: Render ──────────────────────────────────────────────────
        render_group = QGroupBox("Renderizado")
        render_group.setStyleSheet("QGroupBox { font-weight: bold; color: #ccc; }")
        rg_layout = QVBoxLayout(render_group)

        self.cb_helmet   = QCheckBox("Casco 3D")
        self.cb_occluder = QCheckBox("Occluder (Z-buffer)")
        self.cb_helmet.setChecked(True)
        self.cb_occluder.setChecked(True)
        self.cb_helmet.toggled.connect(self.helmet_toggled)
        self.cb_occluder.toggled.connect(self.occluder_toggled)

        rg_layout.addWidget(self.cb_helmet)
        rg_layout.addWidget(self.cb_occluder)
        layout.addWidget(render_group)

        # ── Grupo: Debug ───────────────────────────────────────────────────
        debug_group = QGroupBox("Visualización Debug")
        debug_group.setStyleSheet("QGroupBox { font-weight: bold; color: #ccc; }")
        dg_layout = QVBoxLayout(debug_group)

        self.cb_debug = QCheckBox("Ejes 3D + Landmarks")
        self.cb_mesh  = QCheckBox("Face Mesh completo")
        self.cb_debug.setChecked(True)
        self.cb_debug.toggled.connect(self.debug_toggled)
        self.cb_mesh.toggled.connect(self.mesh_toggled)

        dg_layout.addWidget(self.cb_debug)
        dg_layout.addWidget(self.cb_mesh)
        layout.addWidget(debug_group)

        # ── Métricas ───────────────────────────────────────────────────────
        metrics_group = QGroupBox("Métricas")
        metrics_group.setStyleSheet("QGroupBox { font-weight: bold; color: #ccc; }")
        mg_layout = QVBoxLayout(metrics_group)

        self.lbl_fps       = QLabel("FPS: —")
        self.lbl_detect_ms = QLabel("Detección: — ms")
        self.lbl_render_ms = QLabel("Render: — ms")
        self.lbl_pose_ok   = QLabel("Pose: —")

        for lbl in (self.lbl_fps, self.lbl_detect_ms,
                    self.lbl_render_ms, self.lbl_pose_ok):
            lbl.setFont(QFont("Consolas", 10))
            lbl.setStyleSheet("color: #aed581;")
            mg_layout.addWidget(lbl)

        layout.addWidget(metrics_group)

        # ── Botones de acción ──────────────────────────────────────────────
        btn_restart = QPushButton("↺  Reiniciar cámara")
        btn_restart.setStyleSheet(
            "QPushButton { background: #1e88e5; color: white; border-radius: 6px; "
            "padding: 8px; font-weight: bold; }"
            "QPushButton:hover { background: #1565c0; }")
        btn_restart.clicked.connect(self.camera_requested)
        layout.addWidget(btn_restart)

        btn_quit = QPushButton("✕  Salir  [Q]")
        btn_quit.setStyleSheet(
            "QPushButton { background: #c62828; color: white; border-radius: 6px; "
            "padding: 8px; font-weight: bold; }"
            "QPushButton:hover { background: #b71c1c; }")
        btn_quit.clicked.connect(QApplication.instance().quit)
        layout.addWidget(btn_quit)

        layout.addStretch()

    # ── Actualización de métricas ─────────────────────────────────────────────

    @pyqtSlot(float, float, float, bool)
    def update_metrics(self, fps: float, detect_ms: float,
                        render_ms: float, pose_ok: bool) -> None:
        self.lbl_fps.setText(       f"FPS: {fps:.1f}")
        self.lbl_detect_ms.setText( f"Detección: {detect_ms:.1f} ms")
        self.lbl_render_ms.setText( f"Render: {render_ms:.1f} ms")
        pose_color = "#aed581" if pose_ok else "#ef9a9a"
        self.lbl_pose_ok.setStyleSheet(f"color: {pose_color};")
        self.lbl_pose_ok.setText(   f"Pose: {'OK ✓' if pose_ok else 'No detectada'}")


# ─── Ventana principal ────────────────────────────────────────────────────────

class MainWindow(QMainWindow):
    """Ventana principal de AR-Helmet.

    Combina el preview de cámara con el panel de controles y barra de estado.
    """

    # Señal para comunicar eventos de teclado al orquestador
    key_pressed = pyqtSignal(int)

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("AR-Helmet  |  CS4016 Computación Gráfica")
        self.setMinimumSize(1100, 680)
        self.setStyleSheet("background-color: #1a1a2e; color: #e0e0e0;")
        self._build_ui()
        self._setup_shortcuts()

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QHBoxLayout(central)
        main_layout.setSpacing(8)
        main_layout.setContentsMargins(8, 8, 8, 8)

        # ── Preview de cámara ─────────────────────────────────────────────
        self.preview = CameraPreview()
        main_layout.addWidget(self.preview, stretch=1)

        # ── Panel de controles ────────────────────────────────────────────
        self.controls = ControlPanel()
        self.controls.setStyleSheet(
            "background-color: #16213e; border-radius: 10px; color: #e0e0e0;")
        main_layout.addWidget(self.controls)

        # ── Barra de estado ───────────────────────────────────────────────
        self.status = QStatusBar()
        self.status.setStyleSheet("background-color: #0f3460; color: #90caf9;")
        self.setStatusBar(self.status)
        self.status.showMessage("AR-Helmet listo. Apunta la cámara a tu cara.")

    def _setup_shortcuts(self) -> None:
        QShortcut(QKeySequence("Q"), self).activated.connect(
            QApplication.instance().quit)
        QShortcut(QKeySequence("D"), self).activated.connect(
            lambda: self.controls.cb_debug.toggle())
        QShortcut(QKeySequence("H"), self).activated.connect(
            lambda: self.controls.cb_helmet.toggle())

    # ── API pública ───────────────────────────────────────────────────────────

    def show_frame(self, bgr_frame: np.ndarray) -> None:
        """Actualiza el preview con un nuevo frame BGR."""
        self.preview.update_frame(bgr_frame)

    def update_metrics(self, fps: float, detect_ms: float,
                        render_ms: float, pose_ok: bool) -> None:
        """Actualiza las métricas en el panel lateral."""
        self.controls.update_metrics(fps, detect_ms, render_ms, pose_ok)
        self.status.showMessage(
            f"FPS: {fps:.1f}  |  Detección: {detect_ms:.1f} ms  |  "
            f"Pose: {'OK' if pose_ok else 'no detectada'}"
        )

    def keyPressEvent(self, event) -> None:  # type: ignore[override]
        self.key_pressed.emit(event.key())
        super().keyPressEvent(event)
