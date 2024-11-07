//const {
//    fromZigbeeConverters,
//    toZigbeeConverters,
//    exposes
//} = require('zigbee-herdsman-converters');

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const constants = require('zigbee-herdsman-converters/lib/constants');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const ota = require('zigbee-herdsman-converters/lib/ota');

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

//const fz = {
const fz_local = {	
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
};

//const tz = {
const tz_local = {
    st_text: {
        key: ['stateText'],
        convertSet: async (entity, key, value, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            const payload = {14: {value, type: 0x42}};
            await firstEndpoint.write('genMultistateValue', payload);
            return {state: {stateText: value}};
        },
        convertGet: async (entity, key, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.read('genMultistateValue', ['stateText']);
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
//            fz.st_text,
            fz_local.st_text,
//            fromZigbeeConverters.battery,
            fz.battery,
        ],
        toZigbee: [
//            tz.st_text,
            tz_local.st_text,
//            toZigbeeConverters.factory_reset,
            tz.factory_reset,
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
        },
        exposes: [
            exposes.numeric('battery', ACCESS_STATE).withUnit('%').withDescription('Remaining battery in %').withValueMin(0).withValueMax(100),
            exposes.text('stateText', ACCESS_STATE | ACCESS_WRITE | ACCESS_READ).withDescription('button clicks or data from/to UART'),
        ],
};

module.exports = device;

