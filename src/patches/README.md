# SPDK Patches

This directory contains patches to be applied to the SPDK submodule.

## Patch Naming Convention

Patches should be named with numeric prefixes to ensure proper application order:

- `0001-description.patch`
- `0002-another-fix.patch`
- `0003-feature-name.patch`

## Creating Patches

To create a patch from your modifications:

```bash
cd external/spdk
git diff > ../../src/patches/0001-your-patch-name.patch
```

Or for a specific commit:

```bash
cd external/spdk
git format-patch -1 <commit-hash> --stdout > ../../src/patches/0001-patch-name.patch
```

## Applying Patches

Use the provided script:

```bash
./scripts/apply_patches.sh
```

Or manually:

```bash
cd external/spdk
git apply ../../src/patches/0001-patch-name.patch
```

## Notes

- Always test patches before committing
- Document what each patch does
- Keep patches minimal and focused on a single change

