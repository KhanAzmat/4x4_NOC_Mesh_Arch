#ifndef CONFIG_H
#define CONFIG_H

#define MESH_SIZE_X    4   /* columns  */
#define MESH_SIZE_Y    4   /* rows     */

#define NODES_COUNT    8
#define DMEM_COUNT     8

#define DLM64_SIZE     (32 * 1024)
#define DLM1_512_SIZE  (128 * 1024)
#define DMEM_512_SIZE  (256 * 1024)

#define DMA_CHANNELS   4
#define NOC_BUFFERS    16
#define NOC_LINK_WIDTH 512

#endif /* CONFIG_H */
