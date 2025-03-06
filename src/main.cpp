#include "app.hpp"

int main(int argc, char **argv) {
  aurora::app_t *app = new aurora::app_t{argc, argv};

  app->run();

  delete app;

  return 0;
}
