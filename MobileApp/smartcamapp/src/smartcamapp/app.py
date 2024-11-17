import toga
from toga import Button, MultilineTextInput
from toga.style import Pack
from toga.style.pack import COLUMN, CENTER, ROW, LEFT
from toga.platform import current_platform
import asyncio
import requests  # for sending API requests
import threading
import random
import string
import re
from jnius import autoclass, PythonJavaClass
import select
import socket

################################## Signup ######################################################
def is_android_environment():
    """Determines if the code is running in an Android environment."""
    try:
        if toga.platform.current_platform == 'android':
            # Import Android-specific modules
            from bleekWare.Scanner import Scanner as Scanner
            return True
        else:
            # Import non-Android-specific modules
            from bleak import BleakScanner as Scanner
            from bleak import BleakClient
            return False
    except (ImportError, RuntimeError):
        # If the imports fail, assume it's not Android
        return False

# IS_ANDROID = is_android_environment()
IS_ANDROID = is_android_environment()
print(IS_ANDROID)

# Mock TokenCallback class for non-Android environments
if IS_ANDROID:

    class TokenCallback(PythonJavaClass):
        __javainterfaces__ = ["com/example/firebasebridge/FirebaseHelper$TokenCallback"]

        def __init__(self, on_success, on_error):
            super().__init__()
            self.on_success = on_success
            self.on_error = on_error

        def onTokenReceived(self, token):
            print("Token received:", token)
            self.on_success(token)

        def onError(self, exception):
            print("Error retrieving token:", exception)
            self.on_error(exception)
else:
    class TokenCallback:
        # Mock behavior for desktop testing
        def __init__(self, on_success, on_error):
            self.on_success = on_success
            self.on_error = on_error

        def onTokenReceived(self, token=None):
            if not token:
                token = self.generate_mock_token()
            #print("Mock token received:", token)
            self.on_success(token)

        def onError(self, exception="mock_exception"):
            print("Mock error:", exception)
            self.on_error(exception)

        def generate_mock_token(self):
            return ''.join(random.choices(string.ascii_letters + string.digits, k=152))

class SignupView(toga.Box):
    def __init__(self, switch_to_login):
        super().__init__(style=Pack(direction=COLUMN, padding=10))

        # First Name field
        self.first_name_input = toga.TextInput(placeholder="First Name", style=Pack(padding=(0, 5), width=200))
        self.add(self.first_name_input)

        # Last Name field
        self.last_name_input = toga.TextInput(placeholder="Last Name", style=Pack(padding=(0, 5), width=200))
        self.add(self.last_name_input)

        # Timer for debouncing
        self.username_timer = None

        # Username field
        self.username_input = toga.TextInput(placeholder="Username", style=Pack(padding=(0, 5), width=200))
        self.username_input.on_change = self.check_username_debounced
        self.add(self.username_input)

        # Email field
        self.email_input = toga.TextInput(placeholder="Email", style=Pack(padding=(0, 5), width=200))
        self.email_input.on_change = self.validate_email
        self.add(self.email_input)

        # Password field
        self.password_input = toga.PasswordInput(placeholder="Password", style=Pack(padding=(0, 5), width=200))
        self.password_input.on_change = self.validate_password
        self.add(self.password_input)

        # Confirm Password field
        self.confirm_password_input = toga.PasswordInput(placeholder="Confirm Password", style=Pack(padding=(0, 5), width=200))
        self.confirm_password_input.on_change = lambda widget: self.check_form_validity()
        self.add(self.confirm_password_input)

        # Instructions and feedback labels
        self.feedback_label = toga.Label("", style=Pack(padding=(5, 0)))
        self.add(self.feedback_label)

        self.password_requirements_label = toga.Label("Password must be at least 8 characters, contain a capital letter, a number, and a special character.", style=Pack(padding=(5, 0)))
        self.add(self.password_requirements_label)

        # Signup button
        # Signup button (disabled initially)
        self.signup_button = toga.Button("Signup", on_press=self.signup_user, style=Pack(padding=5), enabled=False)
        self.add(self.signup_button)

        # Back to Log in button
        back_to_login_button = toga.Button("Back to Login", on_press=switch_to_login, style=Pack(padding=5))
        self.add(back_to_login_button)
        #self.switch_to_login = switch_to_login

        # # Feedback label
        # self.feedback_label = toga.Label("", style=Pack(padding=(5, 0)))
        # self.add(self.feedback_label)

    def validate_email(self, widget):
        """ Check if the email is in a valid format. """
        email = self.email_input.value
        email_pattern = r'^[\w\.-]+@[\w\.-]+\.\w{2,3}$'
        if re.match(email_pattern, email):
            self.feedback_label.text = ""
            self.check_form_validity()
        else:
            self.feedback_label.text = "Invalid email format."

    def check_username_debounced(self, widget):
        """Debounce the username check to wait until the user stops typing."""
        if self.username_timer:
            self.username_timer.cancel()  # Cancel any existing timer
        self.username_timer = threading.Timer(1.0, self.check_username)  # Set delay to 500 ms
        self.username_timer.start()

    def check_username(self):
        """Check if the username is unique by sending a request to the backend."""
        username = self.username_input.value
        response = requests.post("http://localhost:5000/api/check_username", json={"username": username})
        if response.status_code == 200 and response.json()["is_unique"]:
            self.feedback_label.text = ""
            self.check_form_validity()
        else:
            suggested_usernames = response.json().get("suggestions", [])
            self.feedback_label.text = f"Username taken. Suggestions: {', '.join(suggested_usernames)}"

    def validate_password(self, widget):
        """ Validate password requirements. """
        password = self.password_input.value
        password_pattern = r'^(?=.*[A-Z])(?=.*\d)(?=.*[@$!%*?&])[A-Za-z\d@$!%*?&]{8,}$'
        if re.match(password_pattern, password):
            self.password_requirements_label.text = ""
            self.check_form_validity()
        else:
            self.password_requirements_label.text = (
                "Password must be at least 8 characters, contain a capital letter, a number, and a special character."
            )

    def check_form_validity(self):
        """ Enable the signup button only if all fields are valid. """
        # Check that all fields are filled and conditions are met
        is_valid = (
                bool(self.first_name_input.value)
                and bool(self.last_name_input.value)
                and bool(self.email_input.value)
                and bool(self.username_input.value)
                and self.password_input.value == self.confirm_password_input.value
                and not self.feedback_label.text  # No username or email errors
                and not self.password_requirements_label.text  # No password requirement errors
        )
        # Enable or disable the signup button based on form validity
        self.signup_button.enabled = is_valid

    def signup_user(self, widget):
        first_name = self.first_name_input.value
        last_name = self.last_name_input.value
        username = self.username_input.value
        email = self.email_input.value
        password = self.password_input.value
        confirm_password = self.confirm_password_input.value

        if password != confirm_password:
            self.feedback_label.text = "Passwords do not match."
            return

        def on_token_received(fcm_token):
            # Send signup request to backend with FCM token
            response = requests.post("http://localhost:5000/api/signup", json={
                "first_name": first_name,
                "last_name": last_name,
                "username": username,
                "email": email,
                "password": password,
                "fcm_token": fcm_token
            })

            if response.status_code == 201:
                self.feedback_label.text = "Signup successful!"
                # Automatically switch to log in view
               # self.switch_to_login()  # No threading delay
            else:
                self.feedback_label.text = "Signup failed. Please try again."

        def on_token_error(exception):
            self.feedback_label.text = "Failed to retrieve FCM token."

        # Define callback and retrieve the token asynchronously
        callback = TokenCallback(on_token_received, on_token_error)

        # Use a mock token on non-Android systems
        if IS_ANDROID:
            # Actual Android token retrieval
            PythonActivity = autoclass('org.kivy.android.PythonActivity')
            context = PythonActivity.mActivity
            FirebaseHelper = autoclass("com.example.firebasebridge.FirebaseHelper")
            helper_instance = FirebaseHelper(context)

            # Retrieve the token asynchronously on Android
            threading.Thread(target=lambda: helper_instance.getToken(callback)).start()
        else:
            # Mock token retrieval for desktop testing
            callback.onTokenReceived(callback.generate_mock_token())  # Simulate successful token retrieval


################################## Login ######################################################
class LoginView(toga.Box):
    def __init__(self, switch_to_signup, switch_to_home):
        super().__init__(style=Pack(direction=COLUMN, padding=10))

        # Username field
        self.username_input = toga.TextInput(placeholder="Username", style=Pack(padding=(0, 5), width=200))
        self.add(self.username_input)

        # Password field
        self.password_input = toga.PasswordInput(placeholder="Password", style=Pack(padding=(0, 5), width=200))
        self.add(self.password_input)

        # Login button
        login_button = toga.Button("Login", on_press=self.login_user, style=Pack(padding=5))
        self.add(login_button)

        # Logout button
        # logout_button = toga.Button("Logout", on_press=logout, style=Pack(padding=5))
        # self.add(logout_button)

        # Signup button
        signup_button = toga.Button("Signup", on_press=switch_to_signup, style=Pack(padding=5))
        self.add(signup_button)

        # Feedback label
        self.feedback_label = toga.Label("", style=Pack(padding=(5, 0)))
        self.add(self.feedback_label)

        # Reference to switch_to_home function
        self.switch_to_home = switch_to_home

        # Jwt Token
        self.auth_token = None

    def get_fcm_token(self, on_success, on_error):
        """ Retrieve the FCM token asynchronously. """
        if IS_ANDROID:
            FirebaseMessaging = autoclass("com.google.firebase.messaging.FirebaseMessaging")
            callback = TokenCallback(on_success, on_error)
            FirebaseMessaging.getInstance().getToken().addOnCompleteListener(callback)
        else:
            # Generate a mock token for desktop testing
            on_success(''.join(random.choices(string.ascii_letters + string.digits, k=152)))

    def login_user(self, widget):
        username = self.username_input.value
        password = self.password_input.value

        def on_token_received(fcm_token):
            response = requests.post("http://localhost:5000/api/login", json={
                "username": username,
                "password": password,
                "fcm_token": fcm_token
            })

            if response.status_code == 200:
                data = response.json()
                self.auth_token = data["access_token"]
                self.feedback_label.text = "Login successful!"
                self.switch_to_home(username)
            else:
                self.feedback_label.text = "Login failed. Please try again."

        def on_token_error(exception):
            self.feedback_label.text = "Failed to retrieve FCM token."

        # Retrieve the FCM token asynchronously
        self.get_fcm_token(on_token_received, on_token_error)

    # Can modify to be any type of action that requires authentication.
    # def access_protected_route(self):
    #     headers = {"Authorization": f"Bearer {self.auth_token}"}
    #     response = requests.get("http://localhost:5000/api/protected_route", headers=headers)
    #
    #     if response.status_code == 200:
    #         self.feedback_label.text = "Access to protected route successful!"
    #     else:
    #         self.feedback_label.text = "Access denied."

################################## Homepage ######################################################
class HomePage(toga.Box):
    def __init__(self, username, parent, logout):
        super().__init__(style=Pack(direction=COLUMN, padding=10))

        # Greeting label with the user's name
        greeting_text = f"Hello, {username}" if username else "Welcome to the Home Page"
        greeting_label = toga.Label(greeting_text, style=Pack(padding=(5, 0)))
        self.add(greeting_label)

        stored_images_button = toga.Button("Stored Images", on_press=parent.open_stored_images, style=Pack(padding=5))
        notifications_button = toga.Button("Notifications", on_press=parent.open_notifications, style=Pack(padding=5))
        image_feed_button = toga.Button("Image Feed", on_press=parent.open_image_feed, style=Pack(padding=5))
        connections_button = toga.Button("Connections", on_press=parent.open_connections, style=Pack(padding=5))

        self.add(stored_images_button)
        self.add(notifications_button)
        self.add(image_feed_button)
        self.add(connections_button)

        # Logout button
        logout_button = toga.Button("Logout", on_press=logout, style=Pack(padding=5))
        self.add(logout_button)


################################## Homepage ######################################################
class StoredImagesWindow(toga.Box):
    def __init__(self, parent):
        super().__init__(style=Pack(direction=COLUMN, padding=10))
        self._parent = parent

        # count to iterate through images
        self.image_count = 0
        self.image_type = "deer"

        button_style = Pack(flex=1)

        self.next_button = toga.Button(
            "Next Image",
            on_press=self.Next_Picture,
            style=button_style)

        self.prev_button = toga.Button(
            "Previous Image",
            enabled=False,
            on_press=self.Prev_Picture,
            style=button_style)

        self.cat_button = toga.Button(
            "Cats",
            enabled=True,
            on_press=self.Cat_Picture,
            style=button_style)

        self.deer_button = toga.Button(
            "Deer",
            enabled=True,
            on_press=self.Deer_Picture,
            style=button_style)

        self.dog_button = toga.Button(
            "Dogs",
            enabled=True,
            on_press=self.Dog_Picture,
            style=button_style)

        self.squirrel_button = toga.Button(
            "Squirrels",
            enabled=True,
            on_press=self.Squirrel_Picture,
            style=button_style)

        # Image view to hold image in stored image window
        self.image_view = toga.ImageView(
            style=Pack(flex=1, width=500, direction=COLUMN, alignment=CENTER)
        )

        self.upper_box = toga.Box(
            style=Pack(direction=ROW),
            children=[
                self.next_button,
                self.prev_button,
            ],
        )
        self.lower_box = toga.Box(
            style=Pack(direction=ROW),
            children=[
                self.cat_button,
                self.dog_button,
                self.squirrel_button,
                self.deer_button,
            ],
        )

        self.image_box = toga.Box(
            style=Pack(direction=ROW, flex=1),
            children=[
                toga.Box(style=Pack(flex=1)),
                self.image_view,
                toga.Box(style=Pack(flex=1)),
            ],
        )
        self.outer_box = toga.Box(
            style=Pack(direction=COLUMN, flex=1),
            children=[
                self.upper_box,
                self.lower_box,
                self.image_box,
            ],
        )

        # self.stored_image_window = toga.MainWindow()
        # self.stored_image_window.content = self.outer_box
        # self.stored_image_window.show()

        self.add(self.outer_box)

        # Example navigation button to return home
        back_button = toga.Button("Back to Home", on_press=self.return_home, style=Pack(padding=5))
        self.add(back_button)

    def return_home(self, widget):
        self._parent.return_to_home()

    def Next_Picture(self, button):
        self.image_count += 1
        self.prev_button.enabled = True
        if (self.image_count > 1):
            self.next_button.enabled = False
        image_being_viewed = "animalPics/" + self.image_type + '/' + str(self.image_count) + ".jpg"
        self.image_view.image = image_being_viewed

    def Prev_Picture(self, button):
        self.image_count -= 1
        self.next_button.enabled = True
        if (self.image_count < 1):
            self.prev_button.enabled = False
        image_being_viewed = "animalPics/" + self.image_type + '/' + str(self.image_count) + ".jpg"
        self.image_view.image = image_being_viewed

    def Deer_Picture(self, button):
        self.image_type = "deer"
        image_being_viewed = "animalPics/deer/" + str(self.image_count) + ".jpg"
        self.image_view.image = image_being_viewed

    def Dog_Picture(self, button):
        self.image_type = "dog"
        image_being_viewed = "animalPics/dog/" + str(self.image_count) + ".jpg"
        self.image_view.image = image_being_viewed

    def Cat_Picture(self, button):
        self.image_type = "cat"
        image_being_viewed = "animalPics/cat/" + str(self.image_count) + ".jpg"
        self.image_view.image = image_being_viewed

    def Squirrel_Picture(self, button):
        self.image_type = "squirrel"
        image_being_viewed = "animalPics/squirrel/" + str(self.image_count) + ".jpg"
        self.image_view.image = image_being_viewed


################################## Homepage ######################################################
class NotificationWindow(toga.Box):
    def __init__(self, parent):
        super().__init__(style=Pack(direction=COLUMN, padding=10))
        self.notification_label = toga.Label("No Notes", style=Pack(text_align=LEFT))
        self._parent = parent

        self.notification_box = toga.Box(
            style=Pack(direction=COLUMN),
            children=[
                self.notification_label
            ],
        )
        # self.notification_window = toga.MainWindow()
        # self.notification_window.content = self.notification_box
        # self.notification_window.show()
        self.add(self.notification_box)

        back_button = toga.Button("Back to Home", on_press=self.return_home, style=Pack(padding=5))
        self.add(back_button)

    def return_home(self, widget):
        self._parent.return_to_home()


################################## Homepage ######################################################
class ConnectionWindow(toga.Box):
    def __init__(self, parent):
        super().__init__(style=Pack(direction=COLUMN, padding=10))
        self._parent = parent
        """Set up the GUI."""
        self.SpecificScan_button = Button(
            'Scan for BLE devices using Specific Address',
            on_press=self.start_scan,
        )

        self.GeneralScan_button = Button(
            'Scan for all BLE devices',
            on_press=self.start_discover,
        )

        self.device_list = MultilineTextInput(
            readonly=True,
            style=Pack(padding=(10, 5), height=200),
        )
        self.data_list = MultilineTextInput(
            readonly=True,
            style=Pack(padding=(10, 5), height=200),
        )
        box = toga.Box(
            children=[
                self.SpecificScan_button,
                self.GeneralScan_button,
                self.device_list,
                self.data_list,
            ],
            style=Pack(direction=COLUMN),
        )

        # self.main_window = toga.MainWindow(
        #     title='BLE SCAN'
        # )
        # self.main_window.content = box
        # self.main_window.show()
        self.add(box)
        self.scan_on = False

        back_button = toga.Button("Back to Home", on_press=self.return_home, style=Pack(padding=5))
        self.add(back_button)

    def return_home(self, widget):
        self._parent.return_to_home()

    async def start_scan(self, widget):
        devices = await Scanner.discover()
        for device in devices:
            print(device)

        # used my phone as an example lol
        address = "08:87:C7:DC:5F:EC"
        async with BleakClient(address) as client:
            if client.is_connected:
                print("Connected to phone!")
                # Perform operations with the connected phone here

    async def start_discover(self, widget):
        self.print_device('Start BLE scan...', clear=True)
        self.print_data(clear=True)

        result = await Scanner.discover(return_adv=True)
        self.print_device('...scanning stopped.')
        self.show_scan_result(result)


    def show_scan_result(self, data):
        #Show names of found devices and attached advertisment data.
        self.print_device('Found devices:')
        self.print_data('Device data:', clear=True)
        for key in data:
            device, adv_data = data[key]
            self.print_device(self.get_name(device))
            self.print_data(f'{device}\n{adv_data}')
            self.print_data()

    def get_name(self, device):
        #Return name or address of BLE device.
        if device.name:
            return device.name
        else:
            return f'No name ({device.address})'

    def print_device(self, device='', clear=False):
        #Write device name to MultilineTextInput for devices.
        if clear:
            self.device_list.value = ''
        self.device_list.value += device + '\n'
        self.device_list.scroll_to_bottom()

    def print_data(self, data='', clear=False):
        # Write device data to MultilineTextInput for device data.
        if clear:
            self.data_list.value = ''
        self.data_list.value += data + '\n'
        self.data_list.scroll_to_bottom()

    def start_socket_server(self):
        def socket_thread():
            s = socket.socket()
            host = socket.gethostname()
            port = 1247
            s.bind((host, port))
            s.listen(5)
            while True:
                client, addr = s.accept()
                print(f"Connection accepted from {addr}")
                client.send("Server approved connection\n".encode())
                data = client.recv(1024).decode()
                print(f"Received: {data}")
                client.close()

        threading.Thread(target=socket_thread, daemon=True).start()


################################## Homepage ######################################################
class ImageFeedWindow(toga.Box):
    def __init__(self, parent):
        super().__init__(style=Pack(direction=COLUMN, padding=10))
        # self.notification_window = toga.MainWindow()
        # self.notification_window.show()
        self._parent = parent
        feed_label = toga.Label("Image Feed", style=Pack(padding=(0, 5)))
        self.add(feed_label)

        back_button = toga.Button("Back to Home", on_press=self.return_home, style=Pack(padding=5))
        self.add(back_button)

    def return_home(self, widget):
        self._parent.return_to_home()

