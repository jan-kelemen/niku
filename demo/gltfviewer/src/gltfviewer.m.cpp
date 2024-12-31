#include <application.hpp>

#include <config.hpp>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    gltfviewer::application_t app{gltfviewer::enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}
