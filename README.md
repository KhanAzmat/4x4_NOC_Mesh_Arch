# Mesh NoC Platform (4×4)

Quick‑start (Linux / macOS):

```bash
git clone <repo> mesh_noc_platform   # or copy this folder
cd mesh_noc_platform
make run                 # build & run full HAL test‑suite
make clean               # remove objects
```

Environment options:

* `TEST=<basic|performance|stress>` – run subset of tests  
* `TRACE=1` – verbose NoC packet trace  
* `HAL=<shared.so>` – load external HAL implementation  

This is a **pure C11** software model (no RTL) that simulates eight RISC‑V tiles
and eight 512‑bit DMEM modules arranged in a 4 × 4 mesh.  
It is intended as a reference platform for validating AI‑generated HAL/driver
code that moves data via on‑chip DMA engines and the mesh NoC.
