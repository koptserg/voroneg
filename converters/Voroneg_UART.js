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
    change_period: {
        // set minAbsoluteChange
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
            fromZigbeeConverters.battery,
        ],
        toZigbee: [
            tz.change_period,
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

            await firstEndpoint.configureReporting('genPowerCfg', genPowerCfgPayload);

        },
        exposes: [
            exposes.numeric('battery', ACCESS_STATE).withUnit('%').withDescription('Remaining battery in %').withValueMin(0).withValueMax(100),
            exposes.numeric('battery_period', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withUnit('min').withDescription('Battery report period (default = 30 min)'),
        ],
};

module.exports = device;