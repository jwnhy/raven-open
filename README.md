Raven: A Novel Kernel Debugging Tool on RISC-V
========================================================

This is the open-sourced prototype of paper "[Raven](https://dl.acm.org/doi/abs/10.1145/3489517.3530583): A Novel Kernel Debugging Tool on RISC-V".
Raven is based on OpenSBI, therefore it can be built with OpenSBI's instruction, please see
[README_SBI.md](./README_SBI.md).

The main source code is inside `./lib/utils/rvbt`, most changes from Raven is in this folder.

> Note that we only tested Raven on SiFive Unleash and QEMU with hard-coded configuration, you might need to change certain configuration to run on other platform or adopt to a new version of OpenSBI.
>
> We are also working with [RustSBI](https://github.com/rustsbi/rustsbi) to port Raven there.

## Citation
```
@inproceedings{10.1145/3489517.3530583,
author = {Lu, Hongyi and Zhang, Fengwei},
title = {Raven: a novel kernel debugging tool on RISC-V},
year = {2022},
url = {https://doi.org/10.1145/3489517.3530583},
doi = {10.1145/3489517.3530583},
series = {DAC '22}
}
```
