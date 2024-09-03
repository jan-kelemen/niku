#include <model_selector.hpp>

#include <cppext_numeric.hpp>

#include <fmt/std.h> // IWYU pragma: keep

#include <imgui.h>

#include <simdjson.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <string_view>
#include <utility>

// IWYU pragma: no_include <fmt/base.h>

gltfviewer::model_selector_t::model_selector_t() : index_path_buffer_{}
{
    auto const wd{std::filesystem::current_path()};
    auto const str{wd.generic_string()};

    using diff_t =
        std::iterator_traits<decltype(str)::iterator>::difference_type;
    std::ranges::copy_n(std::begin(str),
        cppext::narrow<diff_t>(
            std::min(std::size(str), std::size(index_path_buffer_))),
        std::begin(index_path_buffer_));
}

bool gltfviewer::model_selector_t::select_model()
{
    if (ImGui::InputText("model-index.json path",
            index_path_buffer_.data(),
            index_path_buffer_.size()))
    {
        std::filesystem::path const path{std::string_view{index_path_buffer_}};
        if (std::filesystem::exists(path) &&
            std::filesystem::is_regular_file(path))
        {
            spdlog::info("Selected index file {}", path);

            auto json{simdjson::padded_string::load(path.generic_string())};
            std::vector<sample_model_t> samples;

            simdjson::ondemand::parser parser;
            for (auto model : parser.iterate(json))
            {
                sample_model_t sample;
                model["label"].get_string(sample.label);
                model["name"].get_string(sample.name);
                model["screnshot"].get_string(sample.screenshot);

                for (auto tag : model["tags"].get_array())
                {
                    std::string value;
                    tag.get_string(value);
                    sample.tags.push_back(std::move(value));
                }

                for (auto kv : model["variants"].get_object())
                {
                    sample.variants.emplace_back(
                        std::string{kv.unescaped_key().value()},
                        std::string{kv.value().get_string().value()});
                }

                samples.push_back(std::move(sample));
            }

            selected_sample_ = 0;
            samples_ = std::move(samples);
        }
    }

    if (samples_.empty())
    {
        return false;
    }

    auto const& current_sample{
        samples_[cppext::narrow<size_t>(selected_sample_)]};

    char const* model_preview_value{current_sample.label.c_str()};

    if (ImGui::BeginCombo("Model", model_preview_value, 0))
    {
        int i{};
        for (auto const& sample : samples_)
        {
            auto const selected{selected_sample_ == i};

            if (ImGui::Selectable(sample.label.c_str(), selected))
            {
                selected_sample_ = i;
                selected_variant_ = 0;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }

            ++i;
        }
        ImGui::EndCombo();
    }

    if (current_sample.variants.empty())
    {
        return false;
    }

    auto const& current_variant{
        current_sample.variants[cppext::narrow<size_t>(selected_variant_)]};
    char const* const variant_preview{current_variant.name.c_str()};

    if (ImGui::BeginCombo("Variant", variant_preview, 0))
    {
        int i{};
        for (auto const& variant : current_sample.variants)
        {
            auto const selected{selected_variant_ == i};

            if (ImGui::Selectable(variant.name.c_str(), selected))
            {
                selected_variant_ = i;
            }

            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }

            ++i;
        }
        ImGui::EndCombo();
    }

    return ImGui::Button("Load");
}

std::filesystem::path gltfviewer::model_selector_t::selected_model()
{
    auto const& sample{samples_[cppext::narrow<size_t>(selected_sample_)]};
    auto const& variant{
        sample.variants[cppext::narrow<size_t>(selected_variant_)]};

    std::filesystem::path rv{std::string_view{index_path_buffer_}};
    rv.remove_filename();
    rv /= sample.name;
    rv /= variant.name;
    rv /= variant.file;

    return rv;
}
