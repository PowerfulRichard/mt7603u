# MT7603U
## About
MTK 7603U AP&STA driver for linux kernel 2.6 or 3.2
## Install
### AP
```bash
cd AP
make clean
make
```
### STA
#### For linux kernel 3.2
```bash
cd STA
make clean
make
```
#### For linux kernel 2.6
edit `config.mk` from `/STA/os/linux`
change `HAS_CFG80211_SUPPORT=y` to `HAS_CFG80211_SUPPORT=n`