# Soapy SDR
cd $HOME/build

mkdir -p pothosware/
cd pothosware/
git clone https://github.com/pothosware/SoapySDR.git

mkdir -p SoapySDR-build
cd SoapySDR-build

cmake ../SoapySDR -DCMAKE_BUILD_TYPE=Release

echo "Building SoapySDR.."
make -j2 > /dev/null

make install
