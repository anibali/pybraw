import logging

try:
    from termcolor import colored
    _termcolor_available = True
except ImportError:
    _termcolor_available = False


class CustomFormatter(logging.Formatter):
    def __init__(self, use_colours=True):
        super().__init__()
        self.use_colours = use_colours

    def colourify(self, text, color=None, on_color=None, attrs=None):
        if not self.use_colours:
            return text
        return colored(text, color, on_color, attrs)

    def formatMessage(self, record: logging.LogRecord):
        # Create a prefix based on the log level
        if record.levelno == logging.FATAL:
            level_name = 'FATAL'
            color = 'red'
        elif record.levelno == logging.CRITICAL:
            level_name = 'CRIT'
            color = 'red'
        elif record.levelno == logging.ERROR:
            level_name = 'ERROR'
            color = 'red'
        elif record.levelno in {logging.WARNING, logging.WARN}:
            level_name = 'WARN'
            color = 'yellow'
        elif record.levelno == logging.INFO:
            level_name = 'INFO'
            color = 'blue'
        elif record.levelno == logging.DEBUG:
            level_name = 'DEBUG'
            color = 'green'
        else:
            level_name = '?????'
            color = None
        prefix = self.colourify(f'{record.name}[{level_name:5s}]:', color, attrs=['bold'])
        # Include logger name for child loggers only
        if record.name.find('.') >= 0:
            prefix += ' {}:'.format(record.name)
        # Add prefix to every line
        msg = '\n'.join([
            ' '.join([prefix, line])
            for line in record.getMessage().splitlines(keepends=False)
        ])
        return msg


def _setup_logger():
    """Create a logger that plays nicely with tqdm progress indicators.
    """
    logger = logging.getLogger('pybraw')
    logger.setLevel(logging.WARNING)
    handler = logging.StreamHandler()
    handler.setFormatter(CustomFormatter(use_colours=_termcolor_available))
    logger.addHandler(handler)
    return logger


def create_file_handler(filename):
    handler = logging.FileHandler(filename)
    handler.setFormatter(CustomFormatter(use_colours=False))
    return handler


log = _setup_logger()
