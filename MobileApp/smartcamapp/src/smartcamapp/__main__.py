import toga
from smartcamapp.app import LoginView, SignupView, HomePage, StoredImagesWindow, NotificationWindow, ImageFeedWindow, ConnectionWindow


class MainApp(toga.App):
    def startup(self):
        # Set up the main window
        self.main_window = toga.MainWindow(title="Smart Trail Camera App")

        # Load the initial view as LoginView
        self.current_view = LoginView(switch_to_signup=self.switch_to_signup, switch_to_home=self.switch_to_home)
        self.main_window.content = self.current_view
        self.main_window.show()

    def switch_to_signup(self, widget=None):
        # Switch to the SignupView with a reference to the login switch method
        self.current_view = SignupView(switch_to_login=self.switch_to_login)
        self.main_window.content = self.current_view

    def switch_to_login(self, widget=None):
        # Switch back to the LoginView
        self.current_view = LoginView(switch_to_signup=self.switch_to_signup, switch_to_home=self.switch_to_home)
        self.main_window.content = self.current_view

    def switch_to_home(self, username):
        # Switch to the HomePage, passing in the username
        self.current_view = HomePage(username=username, parent=self, logout=self.switch_to_login)
        self.main_window.content = self.current_view

    def logout(self, widget=None):
        # Clear stored token (if implemented) and go back to log in view
        # Here you can reset any authentication details, for example:
        # self.auth_token = None
        self.switch_to_login()

    def open_stored_images(self, widget=None):
        stored_images_view = StoredImagesWindow(parent=self)
        self.main_window.content = stored_images_view

    def open_notifications(self, widget=None):
        notifications_view = NotificationWindow(parent=self)
        self.main_window.content = notifications_view

    def open_image_feed(self, widget=None):
        image_feed_view = ImageFeedWindow(parent=self)
        self.main_window.content = image_feed_view

    def open_connections(self, widget=None):
        connections_view = ConnectionWindow(parent=self)
        self.main_window.content = connections_view

    def return_to_home(self, widget=None):
        # Safely get the username if the current view has it, otherwise use a default or None
        username = getattr(self.current_view, "username", None)
        homepage_view = HomePage(username=username, parent=self, logout=self.switch_to_login)
        self.main_window.content = homepage_view
        self.current_view = homepage_view

def main():
    return MainApp(formal_name="Smart Cam App", app_id="com.example.smartcamapp")

if __name__ == "__main__":
    main().main_loop()
