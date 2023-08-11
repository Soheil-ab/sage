from acme.utils.loggers import aggregators
from acme.utils.loggers import base
from acme.utils.loggers import csv
from acme.utils.loggers import filters
from acme.utils.loggers import terminal
from acme.utils.loggers.tf_summary import TFSummaryLogger


def make_logger(
    directory_or_file: str,
    label: str,
    save_data: bool = True,
    time_delta: float = 1.0,
    use_tb: bool = False,
    use_terminal: bool = False
) -> base.Logger:

    loggers = []

    if save_data:

        loggers.append(csv.CSVLogger(directory_or_file))

    if use_terminal:
        terminal_logger = terminal.TerminalLogger(
            label=label, time_delta=time_delta)
        loggers.append(terminal_logger)

    if use_tb:

        tf_logger = TFSummaryLogger(directory_or_file)
        loggers.append(tf_logger)

    logger = aggregators.Dispatcher(loggers)
    logger = filters.NoneFilter(logger)
    logger = filters.TimeFilter(logger, time_delta)
    return logger
