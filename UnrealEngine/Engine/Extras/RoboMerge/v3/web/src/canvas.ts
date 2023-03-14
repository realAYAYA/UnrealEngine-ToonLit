// function drawCanvas(edgeTableRow: string) {
// 	const canvasDiv = $(edgeTableRow).find(".elist-canvas")
// 	const canvasWidth = canvasDiv.outerWidth(true)
  
// 	// If the div is already collapsed, briefly expand the div so we can accurately draw the canvas
// 	const collapsingDiv = $(edgeTableRow).children(":first")
// 	const startExpanded = $(collapsingDiv).hasClass("show")
// 	const collapsingDivClassesBefore = $(collapsingDiv).attr('class')
// 	if (!startExpanded) {
// 	  $(collapsingDiv).addClass('show');
// 	}
// 	// Try to find an existing canvas element first. Otherwise create a new one
// 	let canvas = canvasDiv.find("canvas");
// 	if (canvas.length === 0) {
// 		canvas = $("<canvas>").appendTo(canvasDiv);
// 	}

// 	// Reset the canvas dimensions on re-draw before we get the div's height.
// 	// This is to ensure zooms don't ever increase the canvas size
// 	canvas.attr("width", 0);
// 	canvas.attr("height", 0);
// 	const canvasHeight = $(collapsingDiv).find("div.etable").first().outerHeight(true);

// 	canvas.attr("width", canvasWidth);
// 	canvas.attr("height", canvasHeight);
// 	const context = canvas[0].getContext("2d");
  
//   	if (canvasHeight > 0) {
//   		// set element to be same size as canvas to prevent scaling
// 		canvas.height(canvasHeight)
// 	}

// 	// First begin by clearing out the canvas. This comes in handy when we need to redraw the element
// 	context.clearRect(0, 0, canvasWidth, canvasHeight);
// 	context.fillStyle = $(edgeTableRow).find('.erow').css('background-color')
// 	context.fillRect(0, 0, canvasWidth, canvasHeight)
  
// 	// Configure line here
// 	context.lineWidth = 2;
// 	context.lineJoin = "miter";
// 	context.shadowBlur = 1;
// 	context.shadowColor = "DimGray";
  
// 	// For each edge, we want to draw a line between the top of the parent row (bottom of the node row) to the edge
// 	let previousY = 0;
// 	const edgeRows = $(edgeTableRow).find(".erow");
// 	edgeRows.each(function(_eRowIndex, edgeRow) {
// 	  const edgeRowHeight = $(edgeRow).outerHeight(true);
// 	  const edgeRowPos = $(edgeRow).position();
// 	  const targetY = edgeRowPos.top + edgeRowHeight / 2;
  
// 	  // Move to the center of the div on the X access, and start where we last left off
// 	  context.moveTo(canvasWidth / 2, previousY);
  
// 	  // Draw to the center of the edge row
// 	  context.lineTo(canvasWidth / 2, targetY);
  
// 	  // Draw over to the row
// 	  context.lineTo(canvasWidth - 5, targetY);
  
// 	  // Draw a short end to the line
// 	  context.moveTo(canvasWidth - 10, targetY - 5);
// 	  context.lineTo(canvasWidth - 5, targetY);
// 	  context.lineTo(canvasWidth - 10, targetY + 5);
  
// 	  // Finally set the previous Y to our completed line
// 	  previousY = targetY;
// 	});
  
// 	// Draw
// 	context.stroke();
  
// 	// If the div was initially collapsed, restore the state
// 	if (!startExpanded) {
// 	  $(collapsingDiv).attr("class", collapsingDivClassesBefore);
// 	}
// }

function drawAllCanvases() {
	// Process each canvas by first finding the Edge Table Row parent
	$('.gtable .elist-row').each(function(_index, edgeTableRow) {
		// drawCanvas(edgeTableRow);
	})
}