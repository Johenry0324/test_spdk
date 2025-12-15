# SPDK Research Project

This project is for researching and experimenting with SPDK (Storage Performance Development Kit).

## Project Structure

```
spdk-research/
├── .gitmodules              # Submodule configuration
├── external/
│   └── spdk/               # SPDK submodule
├── src/
│   ├── experimental/       # Experimental features
│   ├── patches/           # SPDK patches
│   └── tests/             # Test code
├── scripts/
│   ├── setup_env.sh       # Environment setup
│   └── apply_patches.sh   # Apply patches
├── docs/                   # Documentation
├── config/                 # Configuration files
└── CMakeLists.txt          # Root CMake configuration
```

## Getting Started

### Prerequisites

- Git
- CMake (>= 3.10)
- C/C++ compiler (GCC or Clang)
- Python 3 (for SPDK scripts)

### Initial Setup

1. Clone the repository with submodules:
   ```bash
   git submodule update --init --recursive
   ```

2. Set up the environment:
   ```bash
   ./scripts/setup_env.sh
   ```

3. Apply patches (if any):
   ```bash
   ./scripts/apply_patches.sh
   ```

4. Build the project:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

## SPDK Version

- **SPDK Version**: Check `external/spdk/` commit or tag
- **Last Updated**: See `.gitmodules`

## Patches

Patches are stored in `src/patches/` and should be named with numeric prefixes (e.g., `0001-fix-xxx.patch`, `0002-feature-xxx.patch`) to ensure proper application order.

## Documentation

See `docs/` directory for:
- Design documents
- Research notes
- API documentation

## Contributing

1. Create experimental features in `src/experimental/`
2. Add tests in `src/tests/`
3. Document changes in `docs/`

## License

[Add your license information here]

