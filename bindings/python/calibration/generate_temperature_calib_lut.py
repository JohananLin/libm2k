import argparse
import libm2k
import signal
import math
import sys
import os


PATH = 'm2k-calib-temp-lut.ini'
PREFIX = 'cal,temp_lut='


def create_file(calibration_values):
    if os.path.isfile(PATH) is False:
        f = open(PATH, 'w+')
        f.close()

    with open(PATH, mode='r') as file:
        content = file.read()
        content = content.replace(PREFIX, '')
        values = content.split(',')

        while len(values) > 0:
            key = float(values.pop(0))
            parameters = []
            for i in range(8):
                parameters.append(float(values.pop(0)))
            calibration_values[key] = parameters


def write_in_file(calibration_values, nb_values):
    keys = list(calibration_values.keys())
    if 0 < nb_values < len(keys):
        length = float(len(keys))
        valid_keys = []
        for i in range(nb_values):
            valid_keys.append(keys[math.ceil(i * length / nb_values)])

        newdict = {k: calibration_values[k] for k in valid_keys}
        calibration_values = newdict

    with open(PATH, mode='w') as file:
        file.write(PREFIX)

        content = ''
        for key, values in sorted(calibration_values.items()):
            content += ('{0},{1},'.format(key, ','.join(str(round(v, 6)) for v in values)))
        content = content[:-1]
        file.write(content)


def get_key_values(context):
    key = context.getDMM('ad9963').readChannel('temp0').value
    parameters = [context.getAdcCalibrationOffset(0), context.getAdcCalibrationOffset(1),
                  context.getAdcCalibrationGain(0), context.getAdcCalibrationGain(1),
                  context.getDacCalibrationOffset(0), context.getDacCalibrationOffset(1),
                  context.getDacCalibrationGain(0), context.getDacCalibrationGain(1)]

    return key, parameters


def generate_file(ctx, calibration_values, max_temperature, nb_values):
    print("'CTRL + C' to stop the calibration data extraction process")
    create_file(calibration_values)
    i = 0
    while True:
        ctx.calibrateADC()
        ctx.calibrateDAC()

        key, parameters = get_key_values(ctx)
        if key >= max_temperature:
            break

        calibration_values[key] = parameters
        print('Temperature_' + str(i) + ': ' + str(key) + '\u2103')
        i += 1
    write_in_file(calibration_values, nb_values)


def main():
    parser = argparse.ArgumentParser(prog='m2kcli',
                                     description='Calibration control of ADALM2000 from the command line',
                                     usage='%(prog)s [-h | --help] [-v | --version]')

    parser.add_argument('uri',
                        action='store',
                        metavar='uri',
                        type=str,
                        help='describe the context location ')

    parser.add_argument('-t', '--table',
                        action='store',
                        metavar='attr=<val>',
                        nargs='*',
                        type=str,
                        help='generate the lookup table containing the calibration parameters')

    parser.set_defaults(func=parse_arguments)

    args = parser.parse_args()
    args.func(args)


def parse_arguments(args):
    try:
        if args.uri == 'auto':
            ctx = libm2k.m2kOpen()
        else:
            ctx = libm2k.m2kOpen(args.uri)
        if ctx is None:
            raise Exception('Invalid uri')
        print('Connection established')

        if args.table is not None:
            calibration_values = dict()

            max_temperature = 75.0
            nb_values = -1
            for arg in args.table:
                key, value = arg.split('=')
                if key == 'max_temp':
                    max_temperature = float(value)
                elif key == 'nb_values':
                    nb_values = int(value)

            def signal_handler(sig, frame):
                nonlocal calibration_values
                write_in_file(calibration_values, nb_values)
                libm2k.contextClose(ctx)
                sys.exit(0)
            signal.signal(signal.SIGINT, signal_handler)

            generate_file(ctx, calibration_values, max_temperature, nb_values)

        libm2k.contextClose(ctx)
        return 0
    except Exception as error:
        print(error)
        return -1


if __name__ == "__main__":
    main()
