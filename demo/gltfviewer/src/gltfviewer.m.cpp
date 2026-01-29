#include <application.hpp>

#include <cstdlib>

int main(int argc, char const** argv)
{
    gltfviewer::application_t app{argc, argv};
    app.run();
    return EXIT_SUCCESS;
}
