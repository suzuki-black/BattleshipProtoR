/* main.c — エントリ。ここは「起動初期化 → シーンFSM へ委譲」だけ。
   ゲームロジックは書かない(巨大 main() を作らないという規律の起点)。 */
#include "sys.h"
#include "scene.h"

void main(void) {
    sys_init();           /* turboR: R800 ブースト等 */
    scene_run(SC_BOOT);   /* 以降ここから戻らない    */
}
