#include <application.hpp>

#include <config.hpp>

#include <cstdlib>

int main(int argc, char const** argv)
{
    heatx::application_t app{argc, argv, heatx::enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}
