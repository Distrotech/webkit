
import logging
import os
import subprocess

from browser_driver import BrowserDriver


_log = logging.getLogger(__name__)


class OSXBrowserDriver(BrowserDriver):
    bundle_identifier = None
    platform = 'osx'

    def prepare_env(self, device_id):
        self.close_browsers()
        from Quartz import CGWarpMouseCursorPosition
        CGWarpMouseCursorPosition((10, 0))

    def close_browsers(self):
        self._terminiate_processes(self.bundle_identifier)

    @classmethod
    def _launch_process(cls, build_dir, app_name, url, args):
        if not build_dir:
            build_dir = '/Applications/'
        app_path = os.path.join(build_dir, app_name)

        _log.info('Launching "%s" with url "%s"' % (app_path, url))

        # FIXME: May need to be modified for a local build such as setting up DYLD libraries
        args = ['open', '-a', app_path] + args
        cls._launch_process_with_caffinate(args)

    @classmethod
    def _terminiate_processes(cls, bundle_identifier):
        _log.info('Closing all terminating all processes with the bundle identifier %s' % bundle_identifier)
        from AppKit import NSRunningApplication
        processes = NSRunningApplication.runningApplicationsWithBundleIdentifier_(bundle_identifier)
        for process in processes:
            process.terminate()

    @classmethod
    def _launch_process_with_caffinate(cls, args, env=None):
        process = subprocess.Popen(args, env=env)
        subprocess.Popen(["/usr/bin/caffeinate", "-disw", str(process.pid)])
        return process

    @classmethod
    def _screen_size(cls):
        from AppKit import NSScreen
        return NSScreen.mainScreen().frame().size
