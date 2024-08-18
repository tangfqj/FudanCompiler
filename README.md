# Fudan Compiler

This repo implements a compiler for a custom language called FDMJ. The compiler is able to compile FDMJ programs into LLVM instructions and RPi instructions. The LLVM instructions can be executed by `lli`, while the RPi instructions can be executed on `qemu`.

The grammar and specifications of FDMJ can be found [here](docs/FDMJ-Grammar.md).


## 🛠️ Setup
### 📦 Prerequisites
Our compiler is developed and tested on **Ubuntu20.04 LTS**. You may follow the tutorial [here](#Tutorial-of-Installing-Prerequisites) to install the prerequisites.

### 🔨 Build
```angular2html
git clone https://github.com/tangfqj/FudanCompiler.git
cd FudanCompiler
make compile
```

## 🚀 Usage
- If you want to compile all the FDMJ programs under the test directory `test/` into LLVM instructions and run it with `lli`
```angular2html
make run-llvm
```

- If you want to compile all the FDMJ programs under the test directory `test/` into RPi instructions and run it on qemu
```angular2html
make run-rpi
```  
  
- If you want to compile a specific FDMJ program `test/test1.fdmj` into LLVM/RPi and run it accordingly, we provide two extra commands
```angular2html
make run-llvm TEST=test1
make run-rpi TEST=test1
```

- If you want to clean the generated files, you can run
```angular2html
make clean
```

## 💡 Acknowledgement
This project is mainly based on the course homework of Compiler Cource (Honor Track) in Fudan University.

I would like to thank my instructor, Prof. Xiaoyang Wang, as well as TAs, Jiangfan and Yanjun, for their guidance and help.
