# MCU based Process Control Unit (Proof of Concept)

## Getting Started

Development board: *nucleo_f767zi*


1. Follow the instructions at: https://docs.zephyrproject.org/latest/getting_started/index.html
2. Checkout the project to e.g. `${HOME}/Projects/`
3. Open a new terminal in the project root folder and execute the following actions:
```
cmake -GNinja -Bbuild -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBOARD=nucleo_f767zi .
cd build && ninja
```