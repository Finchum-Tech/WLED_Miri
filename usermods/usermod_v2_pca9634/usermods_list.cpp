#include "wled.h"

#ifdef USERMOD_PCA9634
  #include "usermod_pca9634.cpp"
#endif

void registerUsermods() {
  #ifdef USERMOD_PCA9634
    usermods.add(new UsermodPCA9634());
  #endif
}
