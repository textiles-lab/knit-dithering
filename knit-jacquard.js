#!/usr/bin/env node

let fimage = null;
let bimage = null;
let do_bindoff = false;
let usage = false;
let done_with_options = false;
let carriers_json = null;

//parse command line options:
for (let argi = 2; argi < process.argv.length; ++argi) {
  const arg = process.argv[argi];
  if (done_with_options || !arg.startsWith("--")) {
    if (fimage === null) fimage = arg;
    else if (bimage === null) bimage = arg;
    else {
      console.error(`ERROR: extra filename ('${arg}') on the command line.`);
      usage = true;
    }
  } else if (arg === "--bindoff") {
    do_bindoff = true;
  } else if (arg === "--carriers") {
    if (argi + 1 >= process.argv.length) {
      console.error(`ERROR: --carriers should be followed by a filename.`);
      usage = true;
    }
    carriers_json = process.argv[argi+1];
	argi += 1;
  } else if (arg === "--") {
    done_with_options = true;
  } else {
    console.error(`ERROR: unknown option ('${arg}') on the command line.`);
    usage = true;
  }
}

if (fimage === null || bimage === null) {
  console.error("ERROR: must supply a front and back image.");
  usage = true;
}

//if needed, show usage info and quit:
if (usage == true) {
  console.error("Usage:\n\tknit-jacuard.js [--bindoff] [--] <fimage.png> <bimage.png>");
  process.exit(1);
}

console.warn(`Doing a generalized jacquard knit of:\nfront: '${fimage}'\n back: '${bimage}'`);
if (do_bindoff) {
  console.warn(`Will do a bind-off!`);
} else {
  console.warn(`Will not bind-off the knitting.`);
}

const fs = require("fs");
const { format } = require("path/posix");
const PNG = require("pngjs").PNG;

const fpng = PNG.sync.read(fs.readFileSync(fimage));
const bpng = PNG.sync.read(fs.readFileSync(bimage));

console.log(";!knitout-2");
console.log(";;Carriers: 1 2 3 4 5 6 7 8 9 10");

if (fpng.width == !bpng.width || fpng.height == !bpng.height) {
  console.error("image dimension not align");
  process.exit(1);
}

const Width = fpng.width;
const Height = fpng.height;
const YarnRaw = {};
const YarnLabel = {};
const YarnCar = {
  /* will add this laaaater. */
};
let CarDir = new Map();

function addCar(rrggbb, no, label) {
  const R = (rrggbb >> 16) & 0xff;
  const G = (rrggbb >> 8) & 0xff;
  const B = (rrggbb >> 0) & 0xff;
  const key = `${R}, ${G}, ${B}`;
  if (key in YarnRaw) throw new Error(`Yarn Color '${key}}' already added.`);
  YarnRaw[key] = no;
  YarnLabel[key] = label;
}
if (carriers_json !== null) {
	try {
		let info = JSON.parse(fs.readFileSync(carriers_json));
		if (!Array.isArray(info)) {
			throw new Error("Top-level object is not an array.");
		}
		for (let obj of info) {
			if (typeof obj.color === "undefined") {
				throw new Error(`Entry is missing 'color'`);
			}
			if (typeof obj.color !== "string" || !/^#[0-9A-Fa-f]{6}$/.test(obj.color)) {
				throw new Error(`Color '${JSON.stringify(obj.color)}' does not appear to be a '#RRGGBB' hex string.`);
			}
			if (typeof obj.carrier === "undefined") {
				throw new Error(`Entry is missing 'carrier'`);
			}
			if (typeof obj.carrier !== "number" || Math.round(obj.carrier) !== obj.carrier) {
				throw new Error(`Carrier '${JSON.stringify(obj.carrier)}' does not appear to be an integer.`);
			}
			addCar(parseInt(`0x${obj.color.substr(1)}`), obj.carrier, obj.label);
		}
	} catch (e) {
		console.error(`ERROR: failed to read carrier info from '${carriers_json}'; expecting a json array of objects, each of which has a "color" (#RRGGBB color) and "carrier" (integer) member variable.\nReason: ${e}`);
		process.exit(1);
	}
} else {
	//specify where the yarns are currently installed (quoted string is label)
	addCar(0x946136, 5, "1 brown");
	addCar(0xc23220, 3, "2 orange");
	addCar(0x9d0031, -1, "3 magenta");
	addCar(0x964684, -1, "4 pink");
	addCar(0x836b03, 8, "5 olive");
	addCar(0x3e5037, 7, "6 green");
	addCar(0x193a4b, 2, "7 bluegreen");
	addCar(0x5685fd, 9, "8 lightblue");
	addCar(0x31245d, 10, "9 purple");
	addCar(0x000000, 1, "10 black");
	addCar(0x9b979e, 4, "11 gray");
	addCar(0xffffff, 6, "12 white");
}

fPattern = [];
bPattern = [];
for (let h = Height - 1; h >= 0; h--) {
  fLine = [];
  bLine = [];
  for (let w = 0; w < Width; w++) {
    const idx = 4 * (Width * h + w);
    const bidx = 4 * (Width * h + Width - 1 - w);
    const fclr = `${fpng.data[idx + 0]}, ${fpng.data[idx + 1]}, ${
      fpng.data[idx + 2]
    }`;
    function mapColor(clr) {
      if (!(clr in YarnRaw)) throw new Error(`Missing color ${clr}`);
      if (YarnRaw[clr] === -1)
        throw new Error(`Color ${clr} (${YarnLabel[clr]}) is not on the machine; go put it there!`);
      YarnCar[clr] = YarnRaw[clr];
      CarDir.set(YarnCar[clr], "-");
      return YarnCar[clr];
    }
    fLine.push(mapColor(fclr));
    const bclr = `${bpng.data[bidx + 0]}, ${bpng.data[bidx + 1]}, ${
      bpng.data[bidx + 2]
    }`;
    bLine.push(mapColor(bclr));
  }
  fPattern.push(fLine);
  bPattern.push(bLine);
}

const carInUse = Array.from(CarDir.keys());
carInUse.sort((a, b) => a - b);
{ //helpful info:
	let message = ["Yarns used:"];
	for (let clr of Object.keys(YarnLabel)) {
		if (CarDir.has(YarnCar[clr])) {
			message.push(`  ${YarnLabel[clr]} on ${YarnRaw[clr]}`);
		}
	}
	process.stderr.write(message.join("\n") + "\n");
}

console.log("x-sub-roller-number 3");
caston();
console.log("rack 0.25");
knitImage();
console.log("x-sub-roller-number 0");
if (do_bindoff) {
  bindoff();
} else {
  endRows();
}

// knit a few more  last rows
function endRows() {
  const carReverse = Array.from(CarDir.keys());
  carReverse.sort((a, b) => b - a);
  const min = 0;
  const max = Width - 1;
  const f = (max - 1) % 2;
  console.log("rack 0");
  for (let i = 0; i < 3; i++) {
    for (let car of carInUse) {
      if (CarDir.get(car) == "+") {
        for (let n = min; n <= max; n++) {
          if (n % 2 == f) {
            console.log(`knit + b${n} ${car}`);
          } else {
            console.log(`knit + f${n} ${car}`);
          }
        }
        CarDir.set(car, "-");
      } else {
        for (let n = max; n >= min; n--) {
          if (n % 2 == f) {
            console.log(`knit - f${n} ${car}`);
          } else {
            console.log(`knit - b${n} ${car}`);
          }
        }
        CarDir.set(car, "+");
      }
    }
  }
  for (let car of carReverse) {
    console.log(`outhook ${car}`);
  }
  for (let n = min; n <= max; ++n) {
    console.log(`drop f${n}`);
  }
  for (let n = min; n <= max; ++n) {
    console.log(`drop b${n}`);
  }
}

//knit the pattern
function knitImage() {
  for (let h = 0; h < Height; h++) {
    for (let car of carInUse) {
      if (CarDir.get(car) == "-") {
        for (let w = Width - 1; w >= 0; w--) {
          if (bPattern[h][w] == car) {
            console.log(`knit - b${w} ${car}`);
          }
          if (fPattern[h][w] == car) {
            console.log(`knit - f${w} ${car}`);
          }
        }
        console.log(`miss - b-1 ${car}`);
        CarDir.set(car, "+");
      } else if (CarDir.get(car) == "+") {
        for (let w = 0; w <= Width - 1; w++) {
          if (fPattern[h][w] == car) {
            console.log(`knit + f${w} ${car}`);
          }
          if (bPattern[h][w] == car) {
            console.log(`knit + b${w} ${car}`);
          }
        }
        console.log(`miss + b${Width} ${car}`);
        CarDir.set(car, "-");
      }
    }
  }
}

//cast on
function caston() {
  const min = 0;
  const max = Width - 1;
  const f = max % 2;
  let startDir = "-";
  console.log("x-stitch-number 104");
  let first = true;
  for (let car of carInUse) {
    console.log(`inhook ${car}`);
    for (let n = max; n >= min; n--) {
      if (n % 2 == f) {
        console.log(`knit - f${n} ${car}`);
      } else {
        console.log(`knit - b${n} ${car}`);
      }
    }
    for (let n = min; n <= max; n++) {
      if (n % 2 == f) {
        console.log(`knit + b${n} ${car}`);
      } else {
        console.log(`knit + f${n} ${car}`);
      }
    }
    if (first) {
      first = false;
      console.log("x-stitch-number 105");
    }
    // KNIT tubular
    if (startDir == "-") {
      CarDir.set(car, startDir);
      for (let n = max; n >= min; n--) {
        console.log(`knit - f${n} ${car}`);
      }
      for (let n = min; n <= max; n++) {
        console.log(`knit + b${n} ${car}`);
      }
      startDir = "+";
    } else {
      CarDir.set(car, startDir);
      for (let n = max; n >= min; n--) {
        if (n % 2 == f) {
          console.log(`knit - f${n} ${car}`);
        }
      }
      for (let n = min; n <= max; n++) {
        console.log(`knit + b${n} ${car}`);
      }
      for (let n = max; n >= min; n--) {
        if (n % 2 != f) {
          console.log(`knit - f${n} ${car}`);
        }
      }
      startDir = "-";
    }
    console.log(`releasehook ${car}`);
  }
}

function bindoff() {
  const min = 0;
  const max = Width - 1;
  const carReverse = Array.from(CarDir.keys());
  carReverse.sort((a, b) => b - a);
  let lastCar = carReverse[carReverse.length - 1];
  // console.log(carReverse);
  console.log("rack 0");

  for (let k = 0; k < carReverse.length - 1; k++) {
    if (CarDir.get(carReverse[k]) == "-") {
      for (let r = 0; r <= 1; r++) {
        for (let n = max; n >= min; n--) {
          console.log(`knit - f${n} ${carReverse[k]}`);
        }
        for (let n = min; n <= max; n++) {
          console.log(`knit + b${n} ${carReverse[k]}`);
        }
      }
      console.log(`outhook ${carReverse[k]}`);
    } else {
      for (let r = 0; r <= 1; r++) {
        for (let n = min; n <= max; n++) {
          console.log(`knit + f${n} ${carReverse[k]}`);
        }
        for (let n = max; n >= min; n--) {
          console.log(`knit - b${n} ${carReverse[k]}`);
        }
      }
      console.log(`outhook ${carReverse[k]}`);
    }
  }
  console.log("rack 0");
  console.log("x-stitch-number 106");
  if (CarDir.get(lastCar) == "+") {
    for (let n = min; n <= max; n++) {
      const do_tuck = ((n - min) % 3 == 2);
      console.log(`knit + f${n} ${lastCar}`);
      console.log(`xfer f${n} b${n}`);
      console.log(`knit - b${n} ${lastCar}`);
      if (do_tuck) {
        console.log(`miss - f${n-1} ${lastCar}`);
      }
      if (n != max) {
        console.log("rack 1");
        console.log(`xfer b${n} f${n + 1}`);
        console.log("rack 0");
      } else {
        console.log(`xfer b${n} f${n}`);
      }
      if (do_tuck) {
        console.log(`tuck + f${n - 1} ${lastCar}`);
      }
    }
    // knit tag
    console.log("rack 0");
    console.log(`knit - f${max} ${lastCar}`);
    for (i = 1; i <= 4; i++) {
      for (n = max; n <= max + i; n++) {
        console.log(`knit + f${n} ${lastCar}`);
      }
      for (n = max + i; n >= max; n--) {
        console.log(`knit - f${n} ${lastCar}`);
      }
    }
    for (r = 0; r <= 2; r++) {
      for (n = max; n <= max + 4; n++) {
        console.log(`knit + f${n} ${lastCar}`);
      }
      for (n = max + 4; n >= max; n--) {
        console.log(`knit - f${n} ${lastCar}`);
      }
    }
  } else {
    for (let n = max; n >= min; n--) {
      const do_tuck = ((n - min) % 3 == 2);
      console.log(`knit - b${n} ${lastCar}`);
      console.log(`xfer b${n} f${n}`);
      console.log(`knit + f${n} ${lastCar}`);
      if (do_tuck) {
        console.log(`miss + f${n+1} ${lastCar}`);
      }
      if (n != min) {
        console.log("rack 1");
        console.log(`xfer f${n} b${n - 1}`);
        console.log("rack 0");
      }
      if (do_tuck) {
        console.log(`tuck - f${n + 1} ${lastCar}`);
      }
    }
    //knit tag
    console.log("rack 0");
    console.log(`knit + f${min} ${lastCar}`);
    for (i = 1; i <= 4; i++) {
      for (n = min; n >= min + i; n--) {
        console.log(`knit - f${n} ${lastCar}`);
      }
      for (n = min - i; n <= min; n++) {
        console.log(`knit + f${n} ${lastCar}`);
      }
    }
    for (r = 0; r <= 2; r++) {
      for (n = min; n >= min - 4; n--) {
        console.log(`knit - f${n} ${lastCar}`);
      }
      for (n = min - 4; n <= min; n++) {
        console.log(`knit + f${n} ${lastCar}`);
      }
    }
  }
  console.log(`outhook ${lastCar}`);
  for (let n = min - 4; n <= max + 4; ++n) {
    console.log(`drop f${n}`);
  }
  for (let n = min - 4; n <= max + 4; ++n) {
    console.log(`drop b${n}`);
  }
}
