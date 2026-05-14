import json
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QTextEdit, QPushButton, QLabel


class BLEMessageSimulator(QWidget):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("BLE Message Simulator")

        # Create layout
        layout = QVBoxLayout()

        # Create text box
        self.text_box = QTextEdit(self)
        self.text_box.setPlaceholderText("Enter your message here...")
        layout.addWidget(self.text_box)

        # Create button
        self.button = QPushButton("Send", self)
        self.button.clicked.connect(self.on_button_click)
        layout.addWidget(self.button)

        # Create label to display result
        self.result_label = QLabel(self)
        layout.addWidget(self.result_label)

        # Set window layout
        self.setLayout(layout)

    def on_button_click(self):
        message = self.text_box.toPlainText()

        try:
            # Try parsing text as JSON
            json_data = json.loads(message)
            # Compress JSON
            compressed_json = json.dumps(json_data, separators=(',', ':'))
            print(compressed_json)  # Print compressed JSON
            self.result_label.setText("Valid JSON compressed and printed.")
            self.result_label.setStyleSheet("color: green;")  # Set text color to green
        except json.JSONDecodeError:
            self.result_label.setText("Invalid JSON.")
            self.result_label.setStyleSheet("color: red;")  # Set text color to red


# Create application
app = QApplication([])

# Create window
window = BLEMessageSimulator()
window.resize(400, 250)
window.show()

# Run main loop
app.exec_()
