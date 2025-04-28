#include <application.hpp>

#include <config.hpp>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    reshed::application_t app{reshed::enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}
