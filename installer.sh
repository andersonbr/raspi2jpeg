sudo apt-get install -y libjpeg62-turbo-dev
cd ~
git clone https://github.com/andersonbr/raspi2jpeg.git
cd raspi2jpeg
make
sudo make install
cd ..
rm -fr raspi2jpeg
raspi2jpeg -H
