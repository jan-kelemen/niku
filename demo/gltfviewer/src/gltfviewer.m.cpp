#include <application.hpp>

#include <config.hpp>

#include <cstdlib>

int main(int argc, char const** argv)
{
    gltfviewer::application_t app{argc,
        argv,
        gltfviewer::enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}
