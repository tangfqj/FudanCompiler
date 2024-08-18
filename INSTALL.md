## Tutorial of Installing Prerequisites
- Install gcc-10 and g++-10
```angular2html
sudo apt update
sudo apt upgrade
sudo apt install gcc-10 g++-10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100 --slave /usr/bin/g++ g++ /usr/bin/g++-10
# (optional) If you have multiple versions of gcc installed, you can switch between them by running the following command:
sudo update-alternatives --config gcc
```

- Install lex and yacc
```angular2html
sudo apt-get install flex bison
# Check the version of lex and yacc
lex --version # flex 2.6.4
Yacc --version # bison (GNU Bison) 3.5.1
```

- Install clang and llvm (you will have to install them manually on Ubuntu 20.04 LTS)
```angular2html
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main"
sudo apt-get update
sudo apt-get install clang-14
sudo apt-get install llvm-14
clang-14 -v
lli-14 --version
```

- Install cmake
```angular2html
cd $HOME
sudo apt update
sudo apt install -y libssl-dev
wget https://github.com/Kitware/CMake/releases/download/v3.20.0/cmake-3.20.0-linux-x86_64.tar.gz
tar -xzvf cmake-3.20.0-linux-x86_64.tar.gz
echo 'export PATH="$HOME/cmake-3.20.0-linux-x86_64/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
cmake --version
```

- Install ninja
```angular2html
sudo apt-get install ninja-build
```

- Install qemu
```angular2html
sudo apt install ninja-build pkg-config libglib2.0-dev
cd ~/software
wget https://download.qemu.org/qemu-6.2.0.tar.xz
tar xf qemu-6.2.0.tar.xz
cd qemu-6.2.0
mkdir build
cd build
../configure --target-list=arm-linux-user
make -j4
sudo make install
qemu-arm --version
```

