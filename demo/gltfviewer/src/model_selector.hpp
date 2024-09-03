#ifndef GLTFVIEWER_MODEL_SELECTOR_INCLUDED
#define GLTFVIEWER_MODEL_SELECTOR_INCLUDED

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace gltfviewer
{
    //{
    //  "label": "Barramundi Fish",
    //  "name": "BarramundiFish",
    //  "screenshot": "screenshot/screenshot.jpg",
    //  "tags": ["core","testing"],
    //  "variants": {
    //    "glTF-Draco": "BarramundiFish.gltf",
    //    "glTF-Binary": "BarramundiFish.glb",
    //    "glTF": "BarramundiFish.gltf"
    //  }
    //},

    struct [[nodiscard]] sample_variant_t final
    {
        std::string name;
        std::string file;
    };

    struct [[nodiscard]] sample_model_t final
    {
        std::string label;
        std::string name;
        std::string screenshot;
        std::vector<std::string> tags;
        std::vector<sample_variant_t> variants;
    };

    class [[nodiscard]] model_selector_t final
    {
    public:
        model_selector_t();

        model_selector_t(model_selector_t const&) = delete;

        model_selector_t(model_selector_t&&) noexcept = default;

    public:
        ~model_selector_t() = default;

    public:
        [[nodiscard]] bool select_model();

        [[nodiscard]] std::filesystem::path selected_model();

    public:
        model_selector_t& operator=(model_selector_t const&) = delete;

        model_selector_t& operator=(model_selector_t&&) noexcept = default;

    private:
        std::array<char, 256> index_path_buffer_;

        int selected_sample_{};
        int selected_variant_{};
        std::vector<sample_model_t> samples_;
    };
} // namespace gltfviewer
#endif
