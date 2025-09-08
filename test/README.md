# E-Bike Controller Testing Strategy

## Running Tests

```bash
# All tests in one combined file (recommended)
platformio test -e test
platformio test -e test -v
```
## Test Structure

```
test/
├── test_all_modules.cpp          # ✅ Combined test file (ACTIVE)
├── test_mocks.h                  # Mock functions and constants
└── README.md                     # This file
```
