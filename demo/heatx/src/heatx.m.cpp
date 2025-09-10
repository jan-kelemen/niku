#include <application.hpp>

#include <config.hpp>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    heatx::application_t app{heatx::enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}
