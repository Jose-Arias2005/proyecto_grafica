/**
 * @file pybind_wrapper.cpp
 * @brief Puente pybind11: expone el core C++ como módulo Python `ar_helmet_core`.
 *
 * Bindings expuestos:
 *   - PoseEstimator : estimate_pose(rvec_np, tvec_np) → dict con matrix 4×4
 *   - Renderer      : upload_frame(frame_np), set_lighting(...), render(mv_np),
 *                     get_frame() → np.ndarray BGR
 *   - LightEstimator: Interfaz expuesta para interoperación con Python
 *
 * Contrato de datos:
 *   - Todos los arrays NumPy deben ser contiguos (C-order), dtype=float32 o uint8.
 *   - Los frames de cámara son np.ndarray shape (H, W, 3), dtype=uint8, BGR.
 *   - rvec/tvec son np.ndarray shape (3,), dtype=float32.
 *   - La matrix Model-View se retorna como np.ndarray shape (4,4), dtype=float32.
 */
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "PoseEstimator.hpp"
#include "Renderer.hpp"
#include "LightEstimator.hpp"

namespace py = pybind11;
using namespace arhelmet;

// ── Helpers de validación ─────────────────────────────────────────────────────

static void assertContiguous(const py::array& arr, const char* name) {
    if (!(arr.flags() & py::array::c_style)) {
        throw std::invalid_argument(
            std::string(name) + " debe ser un array NumPy contiguo en C-order.");
    }
}

// ── Módulo pybind11 ───────────────────────────────────────────────────────────

PYBIND11_MODULE(ar_helmet_core, m) {
    m.doc() = R"doc(
        ar_helmet_core — Módulo nativo del pipeline OpenGL para AR-Helmet.

        Provee:
          - PoseEstimator : Conversión rvec/tvec → glm::mat4 (OpenGL Model-View)
          - Renderer      : Pipeline GL de 3 pasadas (background, occluder, helmet)
          - LightEstimator: Interfaz de estimación fotométrica (implementar por equipo)
    )doc";

    // ── Struct LightEstimate ──────────────────────────────────────────────────
    py::class_<LightEstimate>(m, "LightEstimate",
        "Resultado de la estimación de iluminación para un frame.")
        .def(py::init<>())
        .def_readwrite("light_dir",     &LightEstimate::lightDir,
            "Dirección de la luz (vec3 normalizado, espacio cámara)")
        .def_readwrite("light_color",   &LightEstimate::lightColor,
            "Color RGB de la luz difusa [0,1]³")
        .def_readwrite("ambient_color", &LightEstimate::ambientColor,
            "Color RGB del término ambiental [0,1]³")
        .def_static("default_estimate", &LightEstimate::defaultEstimate,
            "Retorna valores de iluminación por defecto seguros.");

    // ── PoseEstimator ─────────────────────────────────────────────────────────
    py::class_<PoseEstimator>(m, "PoseEstimator",
        "Convierte rvec/tvec de cv2.solvePnP a matriz Model-View de OpenGL.")
        .def(py::init<float, float, float, float>(),
            py::arg("fx"), py::arg("fy"), py::arg("cx"), py::arg("cy"),
            "Construye el estimador con parámetros intrínsecos de la cámara.")

        .def("build_model_view",
            [](const PoseEstimator& self,
               py::array_t<float, py::array::c_style | py::array::forcecast> rvec_np,
               py::array_t<float, py::array::c_style | py::array::forcecast> tvec_np)
            -> py::array_t<float>
            {
                if (rvec_np.size() != 3 || tvec_np.size() != 3) {
                    throw std::invalid_argument("rvec y tvec deben tener exactamente 3 elementos.");
                }

                // Liberar GIL durante el cálculo (es compute-bound)
                py::gil_scoped_release release;
                const PoseResult result = self.buildModelView(
                    rvec_np.data(), tvec_np.data());
                py::gil_scoped_acquire acquire;

                // Retornar la matriz 4×4 como np.ndarray (4,4) float32
                py::array_t<float> mat_np({4, 4});
                auto buf = mat_np.mutable_unchecked<2>();
                const auto& mv = result.modelView;
                for (int row = 0; row < 4; ++row)
                    for (int col = 0; col < 4; ++col)
                        buf(row, col) = mv[col][row];  // GLM column-major → row-major NumPy

                return mat_np;
            },
            py::arg("rvec"), py::arg("tvec"),
            R"doc(
                Convierte rvec + tvec (de cv2.solvePnP) a matriz Model-View OpenGL.

                Args:
                    rvec: np.ndarray shape (3,), float32 — Vector de Rodrigues.
                    tvec: np.ndarray shape (3,), float32 — Traslación en mm.

                Returns:
                    np.ndarray shape (4, 4), float32 — Matriz Model-View (row-major).
            )doc")

        .def("set_intrinsics", &PoseEstimator::setIntrinsics,
            py::arg("fx"), py::arg("fy"), py::arg("cx"), py::arg("cy"),
            "Actualiza los parámetros intrínsecos en tiempo de ejecución.")

        .def_property_readonly("fx", &PoseEstimator::fx)
        .def_property_readonly("fy", &PoseEstimator::fy)
        .def_property_readonly("cx", &PoseEstimator::cx)
        .def_property_readonly("cy", &PoseEstimator::cy)

        .def_static("canonical_points",
            []() -> py::array_t<float> {
                const auto& pts = PoseEstimator::canonicalPoints();
                py::array_t<float> arr({(int)pts.size(), 3});
                auto buf = arr.mutable_unchecked<2>();
                for (std::size_t i = 0; i < pts.size(); ++i) {
                    buf(i, 0) = pts[i][0];
                    buf(i, 1) = pts[i][1];
                    buf(i, 2) = pts[i][2];
                }
                return arr;
            },
            "Retorna los 6 puntos 3D canónicos del modelo de cara (mm), shape (6,3).");

    // ── LightEstimator ────────────────────────────────────────────────────────
    py::class_<LightEstimator>(m, "LightEstimator",
        "Estimador fotométrico — implementar en LightEstimator.cpp (equipo humano).")
        .def(py::init<>())

        .def("estimate",
            [](const LightEstimator& self,
               py::array_t<uint8_t, py::array::c_style | py::array::forcecast> frame)
            -> LightEstimate
            {
                assertContiguous(frame, "frame");
                if (frame.ndim() != 3 || frame.shape(2) != 3) {
                    throw std::invalid_argument(
                        "frame debe ser shape (H, W, 3), dtype=uint8 (BGR).");
                }
                const int h = static_cast<int>(frame.shape(0));
                const int w = static_cast<int>(frame.shape(1));

                py::gil_scoped_release release;
                return self.estimate(frame.data(), w, h);
            },
            py::arg("frame"),
            "Estima iluminación desde un frame BGR uint8 shape (H,W,3).")

        .def("calibrate",
            [](LightEstimator& self,
               py::array_t<uint8_t, py::array::c_style | py::array::forcecast> frame)
            {
                if (frame.ndim() != 3) {
                    throw std::invalid_argument("frame debe ser (H,W,3).");
                }
                const int h = static_cast<int>(frame.shape(0));
                const int w = static_cast<int>(frame.shape(1));
                py::gil_scoped_release release;
                self.calibrate(frame.data(), w, h);
            },
            py::arg("reference_frame"),
            "Calibra con un frame de referencia neutro (sin cara).")

        .def("last_estimate", &LightEstimator::lastEstimate,
            "Retorna la última estimación calculada.");

    // ── Renderer ──────────────────────────────────────────────────────────────
    py::class_<Renderer>(m, "Renderer",
        "Pipeline OpenGL de 3 pasadas para AR (background / occluder / helmet).")
        .def(py::init<int, int>(),
            py::arg("width"), py::arg("height"),
            "Inicializa el contexto GL offscreen con el tamaño del framebuffer.")

        .def("init",
            [](Renderer& self,
               const std::string& shader_dir,
               const std::string& helmet_path,
               const std::string& occluder_path)
            {
                py::gil_scoped_release release;
                self.init(shader_dir, helmet_path, occluder_path);
            },
            py::arg("shader_dir"),
            py::arg("helmet_path"),
            py::arg("occluder_path"),
            "Carga shaders y modelos desde disco. Llamar una sola vez.")

        .def("set_projection",
            [](Renderer& self, float fx, float fy, float cx, float cy,
               float near_plane, float far_plane) {
                self.setProjection(fx, fy, cx, cy, near_plane, far_plane);
            },
            py::arg("fx"), py::arg("fy"), py::arg("cx"), py::arg("cy"),
            py::arg("near_plane") = 10.0f,
            py::arg("far_plane")  = 10000.0f,
            "Configura proyección perspectiva desde intrínsecos de cámara (mm).")

        .def("upload_frame",
            [](Renderer& self,
               py::array_t<uint8_t, py::array::c_style | py::array::forcecast> frame)
            {
                assertContiguous(frame, "frame");
                if (frame.ndim() != 3 || frame.shape(2) != 3) {
                    throw std::invalid_argument(
                        "frame debe ser shape (H, W, 3), dtype=uint8, BGR.");
                }
                const int h = static_cast<int>(frame.shape(0));
                const int w = static_cast<int>(frame.shape(1));

                py::gil_scoped_release release;
                self.uploadCameraFrame(frame.data(), w, h);
            },
            py::arg("frame"),
            "Sube el frame BGR de la cámara como textura de fondo. Llamar cada frame.")

        .def("set_lighting",
            [](Renderer& self, const LightEstimate& est) {
                self.setLighting(est);
            },
            py::arg("estimate"),
            "Establece parámetros de iluminación del frame actual.")

        .def("render",
            [](Renderer& self,
               py::array_t<float, py::array::c_style | py::array::forcecast> model_view)
            {
                if (model_view.size() != 16) {
                    throw std::invalid_argument(
                        "model_view debe ser shape (4,4) o (16,), float32.");
                }
                // Reconstruir glm::mat4 desde NumPy row-major
                const float* p = model_view.data();
                glm::mat4 mv;
                for (int row = 0; row < 4; ++row)
                    for (int col = 0; col < 4; ++col)
                        mv[col][row] = p[row * 4 + col];

                py::gil_scoped_release release;
                self.render(mv);
            },
            py::arg("model_view"),
            "Ejecuta las 3 pasadas de render. Llamar después de upload_frame().")

        .def("get_frame",
            [](const Renderer& self) -> py::array_t<uint8_t>
            {
                py::gil_scoped_release release;
                std::vector<uint8_t> pixels = self.getRenderedFrame();
                py::gil_scoped_acquire acquire;

                // CORRECCIÓN de dangling pointer:
                // pixels es una variable local; si pasáramos pixels.data()
                // directamente al constructor de py::array_t, NumPy apuntaría
                // a memoria liberada al salir del scope de la lambda (segfault).
                //
                // Solución: alocar el buffer gestionado por NumPy PRIMERO,
                // copiar con memcpy mientras pixels aún está vivo, y retornar.
                py::array_t<uint8_t> result({ self.height(), self.width(), 3 });
                std::memcpy(result.mutable_data(), pixels.data(), pixels.size());
                return result;
            },
            "Recupera el frame renderizado como np.ndarray (H,W,3) uint8 BGR.")

        .def_property_readonly("width",    &Renderer::width)
        .def_property_readonly("height",   &Renderer::height)
        .def_property_readonly("is_ready", &Renderer::isReady);

    // ── Constantes del módulo ─────────────────────────────────────────────────
    m.attr("__version__") = "0.1.0";
    m.attr("OPENGL_VERSION_MAJOR") = 3;
    m.attr("OPENGL_VERSION_MINOR") = 3;
}
