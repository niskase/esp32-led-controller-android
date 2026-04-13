package com.pndevs.esp32ble

import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat.getSystemService
import com.pndevs.esp32ble.ui.theme.Esp32bleTheme
import java.util.UUID
import android.content.Context
import android.content.pm.PackageManager
import android.util.Log
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.unit.dp
import androidx.core.app.ActivityCompat
import java.util.*
import android.Manifest
import android.widget.Toast
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue

class MainActivity : ComponentActivity() {

    // ESP32 configurations
    private val SERVICE_UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val CHAR_UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
    private val DEVICE_NAME = "ESP32C3-LED-Control"

    private var bluetoothGatt: BluetoothGatt? = null
    private var ledCharacteristic: BluetoothGattCharacteristic? = null

    // UI state
    private var connectionStatus = mutableStateOf("Not connected")
    private var pinInput = mutableStateOf("")
    private var isAuthorized = mutableStateOf(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Check for saved PIN code
        val sharedPref = getSharedPreferences("ESP32_CONFIG", Context.MODE_PRIVATE)
        val savedPin = sharedPref.getString("saved_pin", "")

        if (!savedPin.isNullOrEmpty()) {
            pinInput.value = savedPin
        }

        // Ask for permissions
        checkAndRequestPermissions()

        setContent {
            Esp32bleTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    BLEControlScreen()
                }
            }
        }
    }

    @Composable
    fun BLEControlScreen() {
        var status by remember { connectionStatus }
        var pin by remember { pinInput }
        var authorized by remember { isAuthorized }

        Column(
            modifier = Modifier.fillMaxSize().padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            Text(text = "Status: $status", style = MaterialTheme.typography.headlineSmall)
            Spacer(modifier = Modifier.height(32.dp))

            if (status != "Connected") {
                // Find device
                Button(onClick = { startBleScan() }, modifier = Modifier.fillMaxWidth()) {
                    Text("Find and connect ESP32")
                }
            } else if (!authorized) {
                // Ask PIN code
                Text("Please enter PIN code:")
                OutlinedTextField(
                    value = pin,
                    onValueChange = { if (it.length <= 6) pin = it },
                    label = { Text("5-digit code") },
                    modifier = Modifier.fillMaxWidth()
                )
                Spacer(modifier = Modifier.height(8.dp))
                Button(
                    onClick = { sendPin(pin) },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("ACCEPT PIN")
                }
            } else {
                // Control panel (when connected and PIN correct)
                Text("Device control", style = MaterialTheme.typography.titleLarge)
                Spacer(modifier = Modifier.height(16.dp))
                Text("LED")
                Row {
                    Button(onClick = { sendCommand(1) }) { Text("ON")}
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(onClick = { sendCommand(0) }) { Text("OFF")}
                }
                Spacer(modifier = Modifier.height(16.dp))
                Text("RELAY")
                Row {
                    Button(onClick = { sendCommand(3) }) { Text("ON")}
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(onClick = { sendCommand(2) }) { Text("OFF")}
                }
                Text("BUZZER")
                Row {
                    Button(onClick = { sendCommand(5) }) { Text("ON")}
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(onClick = { sendCommand(4) }) { Text("OFF")}
                }
            }
        }
    }

    // Send PIN
    private fun sendPin(pin: String) {
        val gatt = bluetoothGatt ?: return
        val char = ledCharacteristic ?: return

        val paddedPin = if (pin.length < 6) pin.padEnd(6, ' ') else pin
        val data = paddedPin.toByteArray(Charsets.UTF_8)

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
            writeToCharacteristic(gatt, char, data)
            isAuthorized.value = true
            savePin(pin);
        }

    }

    // Save PIN
    private fun savePin(pin: String) {
        val sharedPref = getSharedPreferences("ESP32_CONFIG", Context.MODE_PRIVATE)
        with (sharedPref.edit()) {
            putString("saved_pin", pin)
            apply()
        }
    }

    // Clear saved PIN
    private fun clearSavedPin() {
        val sharedPref = getSharedPreferences("ESP32_CONFIG", Context.MODE_PRIVATE)
        sharedPref.edit().remove("saved_pin").apply()
        pinInput.value = ""
        isAuthorized.value = false
    }

    private fun writeToCharacteristic(gatt: BluetoothGatt, char: BluetoothGattCharacteristic, data: ByteArray) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeCharacteristic(char, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
            } else {
                char.value = data
                gatt.writeCharacteristic(char)
            }
        }
    }

    // --- Bluetooth logics ---

    private fun startBleScan() {
        val adapter = getSystemService(BluetoothManager::class.java).adapter
        if (adapter == null || !adapter.isEnabled) {
            showToast("Turn bluetooth on!")
            return
        }

        if (!hasPermissions()) {
            checkAndRequestPermissions()
            return
        }

        connectionStatus.value = "Looking for device..."
        val scanner = adapter.bluetoothLeScanner

        val scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val name = result.device.name
                if (name == DEVICE_NAME) {
                    Log.d("BLE", "Found: $name")
                    scanner.stopScan(this)
                    connectToDevice(result.device)
                }
            }
            override fun onScanFailed(errorCode: Int) {
                connectionStatus.value = "Scanning failed: $errorCode"
            }
        }

        // Start scanning (ask for permissions once again)
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
            scanner.startScan(scanCallback)
        }
    }

    private fun connectToDevice(device: BluetoothDevice) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) return

        connectionStatus.value = "Connecting..."
        bluetoothGatt = device.connectGatt(this, false, gattCallback)
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                connectionStatus.value = "Connected, looking for services..."
                if (ActivityCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    gatt.discoverServices()
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                connectionStatus.value = "Connection closed"
                ledCharacteristic = null
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(SERVICE_UUID)
                ledCharacteristic = service?.getCharacteristic(CHAR_UUID)
                connectionStatus.value = "Connected"
            }
        }
    }

    private fun sendCommand(cmd: Int) {
        val gatt = bluetoothGatt ?: return
        val char = ledCharacteristic ?: return
        val data = byteArrayOf(cmd.toByte())
        writeToCharacteristic(gatt, char, data)
    }

    // --- Permission handling ---

    private fun hasPermissions(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                    ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun checkAndRequestPermissions() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.ACCESS_FINE_LOCATION)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (!hasPermissions()) {
            ActivityCompat.requestPermissions(this, permissions, 1)
        }
    }

    private fun showToast(msg: String) {
        runOnUiThread { Toast.makeText(this, msg, Toast.LENGTH_SHORT).show() }
    }
}