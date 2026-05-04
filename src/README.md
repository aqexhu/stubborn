# Driver Code

This directory contains driver implementations for the Stubborn UPS HAT family in multiple programming languages.

## Supported Models

- **Eternal**: Supercapacitor-based UPS
- **Balance**: Hybrid capacitor solution
- **Stamina**: Advanced battery-based UPS with variants:
  - **LF**: LiFePO4 battery
  - **NA**: Sodium-ion battery
  - **MAX**: Multichem battery

## Directory Structure

- [`c/`](./c/) - C implementation (qups-guard2)
- [`python/`](./python/) - Python script (qups-guard.py)

## Installation

### Python
```bash
pip install RPi.GPIO
```

### C Library
```bash
cd c
gcc -Wall qups-guard2.c -o qups-guard2 -lpthread -lgpiod
```

## Usage Examples

### Python
```bash
python qups-guard.py 11  # For Balance model with DIP switches 1 and 2 ON
```

### C
```bash
# Run with DIP switch pattern
./qups-guard2 --dip 11 --shutdown-delay 10
```

## DIP Switch Configuration

| Pattern | Model | PFO Pin | LIM Pin | SHD Pin |
|---------|-------|---------|---------|---------|
| 10 | Eternal | 17 | 27 | 22 |
| 01 | Eternal | 23 | 24 | 25 |
| 11 | Balance | 5 | 6 | 26 |
| 111 | Stamina | 4 | 24 | 23 |
| 011 | Stamina | 14 | 18 | 15 |
| 101 | Stamina | 25 | 7 | 8 |
| 001 | Stamina | 17 | 22 | 27 |
| 110 | Stamina | 10 | 11 | 9 |
| 010 | Stamina | 12 | 20 | 16 |
| 100 | Stamina | 19 | 21 | 26 |

## GPIO Monitoring

Both implementations monitor:
- **PFO (Power Fail Output)**: Indicates if main power is available
- **LIM (Limit/Input)**: Indicates energy storage level
- **SHD (Shutdown)**: Controls system shutdown sequence

## Safety Features

- Automatic shutdown when energy level is low
- Configurable shutdown delay
- Syslog logging for monitoring and debugging

## Contributing

When adding new features:
1. Test on all supported Raspberry Pi models
2. Include proper error handling
3. Update documentation

## License

All code is licensed under the same terms as the main project.