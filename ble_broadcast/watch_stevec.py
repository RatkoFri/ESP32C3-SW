#!/usr/bin/env python3
"""Prikaze stevec pritiskov tipke iz BLE oglasevanja ESP32C3-tipka v realnem casu.

Uporablja knjiznico bleak, ki se na Linuxu prikljuci direktno na BlueZ preko
D-Bus in dobi obvestilo takoj, ko naprava odda nov oglasevalni paket - ni
potrebe po rednem "pollanju" ali locenem "bluetoothctl scan on".
"""

import asyncio

from bleak import BleakScanner

DEVICE_NAME = "ESP32C3-tipka"
COMPANY_ID = 0xFFFF  # rezervirano za testiranje, ni za prave izdelke

zadnji_stevec = None


def na_oglasevanje(device, adv_data):
    global zadnji_stevec

    ime = adv_data.local_name or device.name
    if ime != DEVICE_NAME:
        return

    mfg = adv_data.manufacturer_data.get(COMPANY_ID)
    if not mfg or len(mfg) < 4:
        return

    stevec = int.from_bytes(mfg[:4], byteorder="little")
    if stevec != zadnji_stevec:
        zadnji_stevec = stevec
        print(f"stevec pritiskov: {stevec}", flush=True)


async def main():
    print(f"iscem napravo '{DEVICE_NAME}' ...", flush=True)
    scanner = BleakScanner(detection_callback=na_oglasevanje)
    await scanner.start()
    try:
        while True:
            await asyncio.sleep(1)
    finally:
        await scanner.stop()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
