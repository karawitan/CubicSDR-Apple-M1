# Liquid-DSP
cd $HOME/build

mkdir -p jgaeddert/
cd jgaeddert/
git clone https://github.com/jgaeddert/liquid-dsp.git

cd liquid-dsp/
./bootstrap.sh

echo "Configuring liquid-dsp.."
./configure 

echo "Building liquid-dsp.."
make

# will install to /usr/local
sudo make install
