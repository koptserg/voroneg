const {
    fromZigbeeConverters,
    toZigbeeConverters,
    exposes
} = require('zigbee-herdsman-converters');

const bind = async (endpoint, target, clusters) => {
    for (const cluster of clusters) {
        await endpoint.bind(cluster, target);
    }
};

const ACCESS_STATE = 0b001, ACCESS_WRITE = 0b010, ACCESS_READ = 0b100;

const withEpPreffix = (converter) => ({
    ...converter,
    convert: (model, msg, publish, options, meta) => {
        const epID = msg.endpoint.ID;
        const converterResults = converter.convert(model, msg, publish, options, meta) || {};
        return Object.keys(converterResults)
            .reduce((result, key) => {
                result[`${key}_${epID}`] = converterResults[key];
                return result;
            }, {});
    },
});

const postfixWithEndpointName = (name, msg, definition) => {
    if (definition.meta && definition.meta.multiEndpoint) {
        const endpointName = definition.hasOwnProperty('endpoint') ?
            getKey(definition.endpoint(msg.device), msg.endpoint.ID) : msg.endpoint.ID;
        return `${name}_${endpointName}`;
    } else {
        return name;
    }
};

const fz = {
    set_date: {
        cluster: 'genPowerCfg',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
          return {set_date: msg.data.batteryManufacturer};
        },
    },
    st_text: {
        cluster: 'genMultistateValue',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            if (msg.data.hasOwnProperty('presentValue')) {
             result.action = msg.data.presentValue;
            }
            if (msg.data.hasOwnProperty('stateText')) {
              result.stateText = msg.data.stateText;
            }
            return result;
        },
    },
    battery_config: {
        cluster: 'genPowerCfg',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            if (msg.data.hasOwnProperty(0xF003)) {
                result.battery_period = msg.data[0xF003];
            }
            return result;
        },
    },
};

const tz = {
    set_date: {
        // set delay after motion detector changes from occupied to unoccupied
        key: ['set_date'],
        convertSet: async (entity, key, value, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.write('genPowerCfg', {batteryManufacturer: value});
            return {state: {set_date: value}};
        },
        convertGet: async (entity, key, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.read('genPowerCfg', ['batteryManufacturer']);
        },
    },
    action: {
        key: ['action'],
        convertSet: async (entity, key, value, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.write('genMultistateValue', {presentValue: value});
            return {state: {action: value}};
        },
        convertGet: async (entity, key, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.read('genMultistateValue', ['presentValue']);
        },
    },
    st_text: {
        key: ['stateText'],
        convertSet: async (entity, key, value, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
//            await firstEndpoint.write('genMultistateValue', {description: value});
            const payload = {14: {value, type: 0x42}};
            await firstEndpoint.write('genMultistateValue', payload);
            return {state: {stateText: value}};
        },
        convertGet: async (entity, key, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.read('genMultistateValue', ['stateText']);
        },
    },
    change_period: {
        key: ['battery_period'],
        convertSet: async (entity, key, value, meta) => {
            value *= 1;
            const temp_value = value;
            const payloads = {
                battery_period: ['genPowerCfg', {0xF003: {value, type: 0x21}}],
            };
            await entity.write(payloads[key][0], payloads[key][1]);
            return {
                state: {[key]: temp_value},
            };
        },
        convertGet: async (entity, key, meta) => {
            const payloads = {
                battery_period: ['genPowerCfg', 0xF003],
            };
            await entity.read(payloads[key][0], [payloads[key][1]]);
        },
    },
};

const device = {
        zigbeeModel: ['VORONEG_UART'],
        model: 'VORONEG_UART',
        vendor: 'VORONEG',
        description: '[UART](https://github.com/koptserg/voroneg)',
        supports: 'battery',
        fromZigbee: [
            fz.battery_config,
//            fz.set_date,
            fz.st_text,
//            fromZigbeeConverters.ptvo_switch_uart,
            fromZigbeeConverters.battery,
        ],
        toZigbee: [
            tz.change_period,
//            tz.set_date,
            tz.action,
            tz.st_text,
//            toZigbeeConverters.ptvo_switch_uart,
            toZigbeeConverters.factory_reset,
        ],
        meta: {
            configureKey: 1,
            multiEndpoint: true,
        },
        configure: async (device, coordinatorEndpoint) => {
            const firstEndpoint = device.getEndpoint(1);
            await bind(firstEndpoint, coordinatorEndpoint, [
                'genPowerCfg',
                'genMultistateValue',
            ]);

        const genPowerCfgPayload = [{
                attribute: 'batteryVoltage',
                minimumReportInterval: 0,
                maximumReportInterval: 3600,
                reportableChange: 0,
            },
            {
                attribute: 'batteryPercentageRemaining',
                minimumReportInterval: 0,
                maximumReportInterval: 3600,
                reportableChange: 0,
            }
        ];
        const msBindPayload = [{
            attribute: 'presentValue',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }
        ];

//            await firstEndpoint.configureReporting('genPowerCfg', genPowerCfgPayload);
//            await firstEndpoint.configureReporting('genMultistateValue', msBindPayload);

        },
        exposes: [
            exposes.numeric('battery', ACCESS_STATE).withUnit('%').withDescription('Remaining battery in %').withValueMin(0).withValueMax(100),
//            exposes.text('set_date', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withDescription('Set date (format )'),
            exposes.text('action', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withDescription('button clicks or data from/to UART'),
            exposes.text('stateText', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withDescription('button clicks or data from/to UART'),
            exposes.numeric('battery_period', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withUnit('min').withDescription('Battery report period (default = 30 min)'),
        ],
};

module.exports = device;