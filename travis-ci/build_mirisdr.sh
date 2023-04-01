# osmocom mirisdr

cd $HOME/build

mkdir -p osmocom.org
cd $_
git clone https://gitea.osmocom.org/sdr/libmirisdr.git

cd libmirisdr


mkdir -p  build
cd build

echo "Configuring mirisdr ..."
cmake ..

echo "Building mirisdr ..."
make
sudo make install

