local dump = require "dump";
local dump = require "lookup";
local bit = require("bit");

function sleep(sec)
   socket.select(nil, nil, sec)
end

local M = {}

local FXOSC = 32E6
local FSTEP = (FXOSC / bit.lshift(1, 19))
local BITTIMEUS = 31
local BITRATE = (FXOSC * BITTIMEUS / 1E6)

function FREQTOFREG(F)
	return (F * 1E6 / FSTEP + .5) -- calculate frequency word
end

local OPMODE_STDBY = bit.lshift(1, 2); -- 1 << 2

local REGFIFO = 0x00;
local REGOPMODE = 0x01;
local REGDATAMODUL = 0x02;
local REGBITRATE = 0x03;
local REGFDEV = 0x05;
local REGFR = 0x07;
local REGAFCCTRL = 0x0B;
local REGPALEVEL = 0x11;
local REGLNA = 0x18;
local REGRXBW = 0x19;
local REGAFCBW = 0x1A;
local REGOOKPEAK = 0x1B;
local REGOOKAVG = 0x1C;
local REGOOKFIX = 0x1D;
local REGAFCFEI = 0x1E;
local REGAFC = 0x1F;
local REGFEI = 0x21;
local REGDIOMAPPING1 = 0x25;
local REGDIOMAPPING2 = 0x26;
local REGIRQFLAGS1 = 0x27;
local REGIRQFLAGS2 = 0x28;
local REGRSSITHRESH = 0x29;
local REGPREAMBLE = 0x2C;
local REGSYNCCONFIG = 0x2E;
local REGSYNCVALUE1 = 0x2F;
local REGPACKETCONFIG1 = 0x37;
local REGPAYLOADLENGTH = 0x38;
local REGFIFOTHRESH = 0x3C;
local REGPACKETCONFIG2 = 0x3D;
local REGTESTDAGC = 0x6F;
local REGTESTAFC = 0x71;

local OPMODE_STDBY = 1;
local OPMODE_TX = 3;
local OPMODE_RX = 4;

local TXPWR = 13  -- -18..13 dBm

function REG8(adr, val)
	return { adr = adr, val = val };
end

function REG16(adr, val)
	return { { adr = adr, val = (bit.band(bit.rshift(val, 8), 0xFF)) }, { adr = (adr) + 1, val = bit.band(val, 0xFF) } }
end

local rfmcfg = {
	 -- packet mode, OOK, 0<<5 | 1<<3
	REG8(REGDATAMODUL, bit.bor(bit.lshift(0, 5), bit.lshift(1, 3))),
	REG16(REGBITRATE, BITRATE)[1],
	REG16(REGBITRATE, BITRATE)[2],
	-- AfcCtrl: improved routine
	REG8(REGAFCCTRL, 0x20),
	 -- pa power, 1 << 7 & TXPWR + 18
	REG8(REGPALEVEL, bit.bor(bit.lshift(1, 7), (TXPWR + 18))),
	-- highest gain
	REG8(REGLNA, 0x81),
	-- bandwidth 167 kHz
	REG8(REGRXBW, bit.lshift(2, 3)),
	-- OOK mode peak, slowest, 1 << 6 | 3
	REG8(REGOOKPEAK, bit.bor(bit.lshift(1, 6), 3)),
	REG8(REGOOKAVG, 0x80),
	REG8(REGOOKFIX, 115),
	-- AFC
	REG8(REGAFCFEI, bit.lshift(1, 1)),
	-- DIO0 DIO1 DIO2 DIO3
	REG8(REGDIOMAPPING1, bit.bor(bit.lshift(2, 6), bit.lshift(2, 4), bit.lshift(1,2), 0)),
	REG8(REGDIOMAPPING2, 0x07),
	REG8(REGRSSITHRESH, 0xFF),
	REG8(REGSYNCVALUE1 + 0, 0x00),
	REG8(REGSYNCVALUE1 + 1, 0x00),
	REG8(REGSYNCVALUE1 + 2, 0x00),
	REG8(REGSYNCVALUE1 + 3, 0x00),
	REG8(REGSYNCVALUE1 + 4, 0x00),
	REG8(REGSYNCVALUE1 + 5, 0x00),
	REG8(REGSYNCVALUE1 + 6, 0x00),
	REG8(REGSYNCVALUE1 + 7, 0x01),
	-- unlimited length
	REG8(REGPACKETCONFIG1, 0),
	-- unlimited length
	REG8(REGPAYLOADLENGTH, 0),
	-- TX start with FIFO level, level 63, 0 << 7 | 63
	REG8(REGFIFOTHRESH, bit.bor(bit.lshift(0, 7), 63)),
	REG8(REGPACKETCONFIG2, 0),
	REG8(REGTESTDAGC, 0x20)
};

function writeReg(spi, reg, value)
	local tmp = { bit.bor(reg, 0x80), value };
	spi.rw(tmp);
end

function readReg(spi, reg)
	local tmp = { reg, 0x00 };
	local ret = spi.rw(tmp);

	return ret;
end

function waitFlag(spi, reg, flag, condition, wait)
	while(1) do -- wait for data in hardware FIFO
		if(((bit.band(readReg(spi, reg), flag)) ~= 0) == (condition ~= 0)) then
			break;
		end

		if wait > 0 then
			-- sleep(wait/10);
		end
	end
end

function M.callback(obj, nr, pulses)
	if obj == nil then
		return;
	end

	local duration = 1;
	local dio0level = true;
	local maxreclen = 450;
	local spi = pilight.io.spi(0);

	local config = pilight.config();
	local data = config.getData();

	local minrawlen = lookup(data, 'registry', 'hardware', 'RF433', 'minrawlen') or 0;
	local maxrawlen = lookup(data, 'registry', 'hardware', 'RF433', 'maxrawlen') or 0
	local mingaplen = lookup(data, 'registry', 'hardware', 'RF433', 'mingaplen') or 0
	local maxgaplen = lookup(data, 'registry', 'hardware', 'RF433', 'maxgaplen') or 0

	local data = obj.getUserdata();

	if data['length'] == nil then
		data['length'] = 1;
		length = 1;
	end
	if data['pulses'] == nil then
		data['pulses'] = {};
	end

	while(maxreclen > 0) do
		maxreclen = maxreclen - 1;
		-- waitFlag(spi, REGIRQFLAGS2, bit.lshift(1, 6), 1, 150);

		buf = readReg(spi, REGFIFO);
		mask = 0x80;

		while(mask > 0) do
			if((bit.band(buf, mask) ~= 0) ~= (dio0level ~= false)) then
				if(duration > 0) then
					duration = duration * BITTIMEUS;

					data['length'] = data['length'] + 1;
					length = data['length'];
					data['pulses'][length - 1] = duration;
					if length > 512 then
						data['length'] = 1;
						length = 1;
					end

					if duration > mingaplen then
						if length >= minrawlen and
							length <= maxrawlen then

							local event = pilight.async.event();
							event.register(pilight.reason.RECEIVED_PULSETRAIN);
							event.trigger(data());

							length = 1;
							maxreclen = 0;
							mask = 0;
						end
					end
				end
				dio0level = (dio0level == false);
				duration = 1;
			else
				duration = duration + 1;
			end
			mask = bit.rshift(mask, 1);
		end
	end
	startReceive(spi);
end

function setFrequency(freqkHz)
	local frequency = freqkHz / 1000.0;
	if((frequency > 800) and (frequency < 900)) then
		-- return pilight.hardware.RF868;
		return 868;
	elseif((frequency > 430) and (frequency < 440)) then
		-- return pilight.hardware.RF433;
		return 433;
	else
		-- return pilight.hardware.NONE;
		return 0
	end

	return 0;
end

function M.validate()
	local config = pilight.config();
	local platform = config.getSetting("gpio-platform");
	local data = config.getData();
	local obj = nil;

	for x in pairs(data['hardware']['raspyrfm']) do
		if x ~= 'spi-channel' and x ~= 'frequency' then
			error(x .. "is an unknown parameter")
		end
	end

	local channel = lookup(data, 'hardware', 'raspyrfm', 'spi-channel') or 0;
	local freq = lookup(data, 'hardware', 'raspyrfm', 'frequency') or 0;

	if freq == nil then
		error("frequency parameter missing");
	end

	if type(freq) ~= 'number' or setFrequency(freq) == 0 then
		error("only 433920 or 868350 are accepted frequencies");
	end

	if channel == nil then
		error("frequency parameter missing");
	end

	if type(channel) ~= 'number' then
		error("channel should be a numeric value");
	end

	return 1;
end

function setMode(spi, mode)
	writeReg(spi, REGOPMODE, bit.lshift(mode, 2));
	waitFlag(spi, REGIRQFLAGS1, bit.lshift(1, 7), 1, 0);
end

function startReceive(spi)
	setMode(spi, OPMODE_STDBY);
	writeReg(spi, REGSYNCCONFIG, bit.bor(bit.lshift(1, 7), bit.lshift(7, 3)));
	setMode(spi, OPMODE_RX);
end

function M.send(obj, reason, data)
	if reason ~= pilight.reason.SEND_CODE then
		return;
	end

	local spi = pilight.io.spi(0);
	local bitmask = 0x80;
	local bitstate = 1;
	local fifobuf = 0;
	local i = 1;
	local code = data['pulses'];
	local rawlen = data['rawlen'];
	local repeats = data['txrpt'];
	local pulsebits = math.floor((code[1] + math.floor(BITTIMEUS / 2)) / BITTIMEUS);

	writeReg(spi, REGSYNCCONFIG, 0);
	writeReg(spi, REGOPMODE, bit.lshift(OPMODE_TX, 2));

	while pulsebits > 0 do
		if bitstate then
			fifobuf = bit.bor(fifobuf, bitmask);
		end

		bitmask = bit.rshift(bitmask, 1);
		if bitmask == 0 then
			bitmask = 0x80;
			waitFlag(spi, REGIRQFLAGS2, bit.lshift(1, 7), 0, 100); -- wait for space in hardware FIFO
			writeReg(spi, REGFIFO, fifobuf);
			fifobuf = 0;
		end

		pulsebits = pulsebits - 1;

		if pulsebits == 0 then
			bitstate = not bitstate;

			i = i + 1;
			if i <= rawlen then -- still data in source left?
				pulsebits = math.floor((code[i] + math.floor(BITTIMEUS / 2)) / BITTIMEUS);
			else
				if repeats > 0 then
					i = 1;
					pulsebits = math.floor((code[1] + math.floor(BITTIMEUS / 2)) / BITTIMEUS);
					repeats = repeats - 1;
				end
			end
		end
	end

	waitFlag(spi, REGIRQFLAGS2, bit.lshift(1, 3), 1, 100); -- wait for frame to be sent out completely
	startReceive(spi);
end

function M.run()
	local config = pilight.config();
	local platform = config.getSetting("gpio-platform");
	local data = config.getData();
	local obj = nil;

	local channel = lookup(data, 'hardware', 'raspyrfm', 'spi-channel') or 0;
	local freq = lookup(data, 'hardware', 'raspyrfm', 'frequency') or 0;

	local gpio_dio0 = 1;

	local obj = wiringX.setup(platform);
	local spi = pilight.io.spi(0, 2500000);

	for i = 1, #rfmcfg, 1 do
		writeReg(spi, rfmcfg[i]['adr'], rfmcfg[i]['val']);
	end

	local frequency = setFrequency(freq);

	local freqword = FREQTOFREG(frequency); -- MHz

	writeReg(spi, REGFR, bit.band(bit.rshift(freqword, 16), 0xFF));
	writeReg(spi, REGFR + 1, bit.band(bit.rshift(freqword, 8), 0xFF));
	writeReg(spi, REGFR + 2, bit.band(freqword, 0xFF));

	startReceive(spi);

	obj.pinMode(gpio_dio0, wiringX.PINMODE_INPUT);
	obj.ISR(gpio_dio0, wiringX.ISR_MODE_RISING, "callback", 10);

	local event = pilight.async.event();
	event.register(pilight.reason.SEND_CODE);
	event.setCallback("send");

	return 1;
end

function M.implements()
	local config = pilight.config();
	local platform = config.getSetting("gpio-platform");
	local data = config.getData();
	local obj = nil;

	local channel = lookup(data, 'hardware', 'raspyrfm', 'spi-channel') or 0;
	local freq = lookup(data, 'hardware', 'raspyrfm', 'frequency') or 0;

	local frequency = setFrequency(freq);

	return frequency;
end

function M.info()
	return {
		name = "raspyrfm",
		version = "1.0",
		reqversion = "8.0",
		reqcommit = "";
	}
end

return M;