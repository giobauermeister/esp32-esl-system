const MQTT_BROKER = "ws://localhost:9001";
// const MQTT_BROKER = "ws://broker.emqx.io:8083/mqtt";

const line1Input = document.getElementById("line1Input");
const line2Input = document.getElementById("line2Input");
const line3Input = document.getElementById("line3Input");

const line1Overlay = document.getElementById("line1");
const line2Overlay = document.getElementById("line2");
const line3Overlay = document.getElementById("line3");

const priceInput = document.getElementById("priceInput");
const priceOverlay = document.getElementById("price");

const tagIdInput = document.getElementById("tagIdInput");
const tagIdOverlay = document.getElementById("tagid");

const MAX_VISIBLE_CHARS = 20;

function stripFormatting(text) {
  return text.replace(/\*\*/g, ""); // remove all **
}

function enforceMaxLength(inputEl) {
  const rawValue = inputEl.value;
  const plainText = stripFormatting(rawValue);

  if (plainText.length > MAX_VISIBLE_CHARS) {
    // Remove extra chars while keeping formatting intact
    let visibleCount = 0;
    let result = '';
    for (let i = 0; i < rawValue.length; i++) {
      if (rawValue[i] === '*' && rawValue[i + 1] === '*') {
        result += '**';
        i++; // skip the next *
        continue;
      }
      if (visibleCount < MAX_VISIBLE_CHARS) {
        result += rawValue[i];
        visibleCount++;
      } else {
        break;
      }
    }
    inputEl.value = result;
  }
}

function applyRichText(inputValue) {
    // Simple replacement: **text** -> <strong>text</strong>
    return inputValue.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>');
}

line1Input.addEventListener("input", () => {
    enforceMaxLength(line1Input);
    line1Overlay.innerHTML = applyRichText(line1Input.value);
});

line2Input.addEventListener("input", () => {
    enforceMaxLength(line2Input);
    line2Overlay.innerHTML = applyRichText(line2Input.value);
});

line3Input.addEventListener("input", () => {
    enforceMaxLength(line3Input);
    line3Overlay.innerHTML = applyRichText(line3Input.value);
});

priceInput.addEventListener("input", () => {
    const value = parseFloat(priceInput.value || 0).toFixed(2);
    const [intPart, decimalPart] = value.split(".");
    priceOverlay.innerHTML = `$${intPart}<span class="decimal">${decimalPart}</span>`;
});

tagIdInput.addEventListener("input", () => {
  tagIdOverlay.textContent = tagIdInput.value;
});

async function createDescriptionCanvas() {
    const editorEl = document.getElementById("editor");
  
    // Render the entire editor image at native resolution
    const scale = 1;
    const canvas = await html2canvas(editorEl, {
      backgroundColor: null,
      scale: scale,
      width: 416,
      height: 240
    });
  
    // Define the description crop region
    const cropX = 10;         // left padding
    const cropY = 80;         // top of first line
    const cropWidth = 215;    // enough for 3 short lines
    const cropHeight = 92;    // 3 lines with spacing
  
    // Create a cropped canvas
    const croppedCanvas = document.createElement("canvas");
    croppedCanvas.width = cropWidth;
    croppedCanvas.height = cropHeight;
  
    const ctx = croppedCanvas.getContext("2d");
    ctx.drawImage(
      canvas,
      cropX, cropY, cropWidth, cropHeight,
      0, 0, cropWidth, cropHeight
    );
  
    console.log(`✅ Exported description area: ${cropWidth}x${cropHeight}`);
    return croppedCanvas;
}

async function createPriceCanvas() {
    const editorEl = document.getElementById("editor");
  
    // Render the entire editor at native resolution
    const scale = 1;
    const canvas = await html2canvas(editorEl, {
      backgroundColor: null,
      scale: scale,
      width: 416,
      height: 240
    });
  
    // Define crop area based on known pixel position of the price tag
    const cropX = 265;
    const cropY = 90;
    const cropWidth = 121;
    const cropHeight = 58;
  
    // Crop only the price tag region
    const croppedCanvas = document.createElement("canvas");
    croppedCanvas.width = cropWidth;
    croppedCanvas.height = cropHeight;
  
    const ctx = croppedCanvas.getContext("2d");
    ctx.drawImage(
      canvas,
      cropX, cropY, cropWidth, cropHeight, // source
      0, 0, cropWidth, cropHeight          // destination
    );
  
    console.log(`✅ Exported price area: ${cropWidth}x${cropHeight}`);
    return croppedCanvas;
}

function canvasToBin(canvas) {
  const width = canvas.width;
  const height = canvas.height;
  const ctx = canvas.getContext("2d");
  const imgData = ctx.getImageData(0, 0, width, height).data;

  const binData = [];

  for (let x = 0; x < width; x++) {
    for (let byteRow = 0; byteRow < Math.ceil(height / 8); byteRow++) {
      let byte = 0;
      for (let bit = 0; bit < 8; bit++) {
        const y = byteRow * 8 + bit;
        if (y >= height) continue;

        const pixelIndex = (y * width + x) * 4;
        const r = imgData[pixelIndex];
        const g = imgData[pixelIndex + 1];
        const b = imgData[pixelIndex + 2];
        const avg = (r + g + b) / 3;

        if (avg < 150) {
          byte |= (1 << (7 - bit)); // Top pixel is MSB
        }
      }
      binData.push(byte);
    }
  }

  return new Uint8Array(binData);
}

function downloadBin(data, filename) {
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
}

function showStatusMessage(text) {
  const container = document.getElementById("statusMessage");
  const message = document.createElement("div");
  message.textContent = text;
  message.style.background = "#65a35f";
  message.style.color = "#fff";
  message.style.padding = "10px 16px";
  message.style.marginTop = "10px";
  message.style.borderRadius = "6px";
  message.style.boxShadow = "0 2px 6px rgba(0, 0, 0, 0.3)";
  message.style.fontSize = "18px";
  message.style.transition = "opacity 0.3s ease";

  container.appendChild(message);

  setTimeout(() => {
    message.style.opacity = "0";
    setTimeout(() => container.removeChild(message), 300);
  }, 5000); // 5 seconds visible
}


async function updateESL() {
    const descCanvas = await createDescriptionCanvas();
    const priceCanvas = await createPriceCanvas();

    const descBin = canvasToBin(descCanvas);
    const priceBin = canvasToBin(priceCanvas);

    // Optionally download locally for testing
    // downloadBin(descBin, "description.bin");
    // downloadBin(priceBin, "price.bin");

    const tagId = document.getElementById("tagIdInput").value || tagIdOverlay.textContent;

    const mqttTopicDesc = `esl/${tagId}/description`;
    const mqttTopicPrice = `esl/${tagId}/price`;

    // Connect to local or public broker via WebSocket
    const client = mqtt.connect(MQTT_BROKER);

    client.on("connect", () => {
      console.log("✅ MQTT connected!");
    
      console.log(`Sending description (${descBin.length} bytes) to ${mqttTopicDesc}`);
      client.publish(mqttTopicDesc, descBin);
      showStatusMessage(`Sending description (${descBin.length} bytes) to ${mqttTopicDesc}`);

      console.log(`Sending price (${priceBin.length} bytes) to ${mqttTopicPrice}`);
      client.publish(mqttTopicPrice, priceBin);
      showStatusMessage(`Sending price (${priceBin.length} bytes) to ${mqttTopicPrice}`);

      showStatusMessage(`Done updating ESL!`);

      setTimeout(() => client.end(), 500);
    });    

    client.on("error", (err) => {
      console.error("❌ MQTT error:", err);
    });
}
