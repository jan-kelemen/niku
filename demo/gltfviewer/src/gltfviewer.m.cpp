#include <application.hpp>

#include <cstdlib>

namespace
{
#ifdef NDEBUG
    constexpr bool enable_validation_layers{false};
#else
    constexpr bool enable_validation_layers{true};
#endif
} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    gltfviewer::application_t app{enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}