# Experimental Features

This directory contains experimental and research code that extends or modifies SPDK functionality.

## Structure

Organize experimental features by category:

- `nvmf/` - NVMe-oF related experiments
- `bdev/` - Block device experiments
- `nvme/` - NVMe driver experiments
- `performance/` - Performance optimization experiments

## Guidelines

1. Each experimental feature should be in its own subdirectory
2. Include a README.md explaining the experiment
3. Add tests in `../tests/` if applicable
4. Document findings in `../../docs/`

## Example Structure

```
experimental/
├── nvmf/
│   └── custom_transport/
│       ├── README.md
│       └── custom_transport.c
└── bdev/
    └── new_bdev_module/
        ├── README.md
        └── new_bdev.c
```

