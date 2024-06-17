import asyncio
from bleak import BleakClient, BleakScanner
import re
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

name = "Croxel Accel Demo"

service_uuid = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
tx_char_uuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# Global variable to store accelerometer data
accel_data = [0, 0, 0]

def notification_handler(sender, data):
    global accel_data
    # Get the data as "[00:01:06.553,161] <inf> main: X: -7.967232, Y: -0.689472, Z: 5.285952"
    data = data.decode("utf-8")
    data = re.search(r"X: ([0-9.-]+), Y: ([0-9.-]+), Z: ([0-9.-]+)", data)
    if data is not None:
        x, y, z = data.groups()
        accel_data = [float(x), float(y), float(z)]
        print(f"X: {x}, Y: {y}, Z: {z}")

async def main():
    device = await BleakScanner.find_device_by_name(name)
    if device is None:
        print(f"{name} not found")
        return

    async with BleakClient(device, services=[service_uuid]) as client:
        print(f"Connected to {name}")
        await asyncio.sleep(1)
        
        nus = None

        for service in client.services:
            if service.uuid == service_uuid:
                nus = service

        if nus is None:
            print("Could not find NUS")
            return
        print(nus)

        tx = None
        
        for characteristic in nus.characteristics:
            if characteristic.uuid == tx_char_uuid:
                tx = characteristic

        if tx is None:
            print("Could not find TX characteristic")
            return

        print(tx)

        await client.start_notify(tx, notification_handler)
        while True:
            await asyncio.sleep(1)

def update_plot():
    global accel_data
    x, y, z = accel_data
    ax.cla()  # Clear the current plot
    ax.quiver(0, 0, 0, x, y, z, length=1.0)
    plt.draw()
    plt.pause(0.1)

if __name__ == "__main__":
    # Initialize plot
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')
    
    # Start BLE and plotting
    loop = asyncio.get_event_loop()
    loop.run_in_executor(None, asyncio.run, main())
    
    # Update the plot in a loop
    while True:
        update_plot()

