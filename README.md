# gta_ban_bypass.prx

Liest /data/GoldHEN/plugins/gta_ban_bypass/ids.conf und ersetzt im
GTA-V-Prozess die libkernel-Exporte fuer OpenPsId und PSID.

## Build
1. ps4-payload-sdk + GoldHEN_Plugins_SDK bereitstellen
2. `make PS4SDK=/pfad GOLDHEN_SDK=/pfad`
3. gta_ban_bypass.prx per FTP nach /data/GoldHEN/plugins/

## plugins.ini
[CUSA00419] /data/GoldHEN/plugins/gta_ban_bypass.prx=true
[CUSA00411] /data/GoldHEN/plugins/gta_ban_bypass.prx=true

## Debug
klog auf Port 3232 mithoeren (nc 192.168.x.x 3232).