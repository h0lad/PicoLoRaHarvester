function decodeUplink(input) {
  var data = {};
  var warnings = [];
  var errors = [];

  var bytes = input.bytes;
  if (!bytes || bytes.length < 15) {
    errors.push("payload too short: " + (bytes ? bytes.length : 0) + " bytes (expected 15)");
    return { data: data, warnings: warnings, errors: errors };
  }

  var pos = 0;

  function take(nbits) {
    var value = 0;
    for (var i = 0; i < nbits; i++) {
      value = (value << 1) | ((bytes[pos >> 3] >> (7 - (pos & 7))) & 1);
      pos++;
    }
    return value;
  }

  function takeSigned(nbits) {
    var v = take(nbits);
    var sign = 1 << (nbits - 1);
    return (v & sign) ? v - (1 << nbits) : v;
  }

  data.current = {
    min_ua: take(15),
    avg_ua: take(15),
    max_ua: take(15),
  };
  data.voltage = {
    min_mv: take(13),
    avg_mv: take(13),
    max_mv: take(13),
  };
  data.temperature_avg_c = takeSigned(12) / 10;
  data.humidity_avg_pct = take(10) / 10;
  data.mcu_temperature_c = takeSigned(12) / 10;

  return { data: data, warnings: warnings, errors: errors };
}
