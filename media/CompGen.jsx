function GenSwap() {
	/// Config
	var Alignments = [
		//32,
		//16,
		8,
		4,
		2,
		1
	];
	var OpNames = [
		//"_mm256_shuffle_epi8",
		//"_mm_shuffle_epi8",
		"bswap64",
		"bswap32",
		"bswap16",
		"Naïve"
	];

	var Size = [540, 480];
	var CellWidth = 24;
	var CellColumns = 9;
	var CellRows = 7;
	var CellCount = CellColumns * CellRows;
	var RegisterCellCount = 8;

	var SwapDuration = 3.0;
	var TotalSwaps = 0;
	var RemainingSwaps = Math.floor(CellCount / 2);
	Alignments.slice().sort().reverse().map(function (CurAlign, i) {
		TotalSwaps += Math.floor(RemainingSwaps / CurAlign);
		RemainingSwaps %= CurAlign;
	});

	// Comp Items
	var CurProj = (app.project) ? app.project : app.newProject();
	var CurComp = CurProj.items.addComp(
		"Swap (" + CellColumns + "x" + CellRows + ")"
		+ "/(" + Alignments.join("|") + ")-" + RegisterCellCount,
		Math.floor(Size[0]),
		Math.floor(Size[1]),
		1,
		SwapDuration * TotalSwaps,
		25
	);
	CurComp.bgColor = [1, 1, 1];
	var LogLayer1 = CurComp.layers.addText();
	var LogLayer2 = CurComp.layers.addText();

	/// Log Text
	LogLayer1.name = "Log1";
	LogLayer2.name = "Log2";
	LogLayer1.position.setValue([
		Size[0] / 2,
		Size[1] * 1 / 16
	]);
	LogLayer2.position.setValue([
		Size[0] / 2,
		Size[1] * 15 / 16
	]);

	var LogProp1 = LogLayer1.property("Text").property("Source Text");
	LogProp1.setValueAtTime(0, "...");
	var LogProp2 = LogLayer2.property("Text").property("Source Text");
	LogProp2.setValueAtTime(0, "...");

	function Log(Message, Time, FadeTime) {
		LogProp1.setValueAtTime(Time, new TextDocument(Message));
		LogProp2.setValueAtTime(Time, new TextDocument(Message));
		var Keys = [[
			Time,
			Time + FadeTime * 1 / 9,
			Time + FadeTime * 8 / 9,
			Time + FadeTime
		],
		[
			0,
			100,
			100,
			0
		]];
		LogLayer1.opacity.setValuesAtTimes(
			Keys[0], Keys[1]
		);
	}

	/// Functions
	function AddRegister(Name) {
		var NewNull = CurComp.layers.addNull();
		NewNull.name = Name;
		NewNull.anchorPoint.setValue([
			NewNull.width / 2, NewNull.height / 2
		]);
		NewNull.scale.setValue([10, 10]);
		var RegShapeLayer = CurComp.layers.addShape();
		RegShapeLayer.parent = NewNull;

		// Register Cells
		var ResCellShapeContents = RegShapeLayer
			.property("Contents")
			.addProperty("ADBE Vector Group")
			.property("Contents");

		for (var i = 0; i < RegisterCellCount; ++i) {
			var ResCellRect = ResCellShapeContents
				.addProperty("ADBE Vector Shape - Rect");
			ResCellRect.property("ADBE Vector Rect Position")
				.setValue([
					i * CellWidth,
					0
				]);
			ResCellRect.property("ADBE Vector Rect Size")
				.setValue([
					CellWidth - CellWidth / 10,
					CellWidth - CellWidth / 10
				]);
			ResCellRect.property("ADBE Vector Rect Roundness")
				.setValue(CellWidth / 10);
		}

		ResCellShapeContents
			.addProperty("ADBE Vector Graphic - Fill")
			.property("ADBE Vector Fill Color")
			.setValue([0.44, 0.44, 0.44]);


		// Register Box
		var ResBoxShapeContents = RegShapeLayer
			.property("Contents")
			.addProperty("ADBE Vector Group")
			.property("Contents");

		var ResBoxRect = ResBoxShapeContents.addProperty("ADBE Vector Shape - Rect")
		ResBoxRect.property("ADBE Vector Rect Size")
			.setValue([
				CellWidth * RegisterCellCount + CellWidth / 5,
				CellWidth + CellWidth / 5
			]);
		ResBoxRect.property("ADBE Vector Rect Position")
			.setValue([(RegisterCellCount / 2 - 1) * CellWidth + CellWidth / 2, 0]);
		ResBoxRect.property("ADBE Vector Rect Roundness")
			.setValue(CellWidth / 10);

		ResBoxShapeContents
			.addProperty("ADBE Vector Graphic - Fill")
			.property("ADBE Vector Fill Color")
			.setValue([0.33, 0.33, 0.33]);
		return NewNull;
	}

	function AddCell(Name, Color) {
		var NewNull = CurComp.layers.addNull();
		NewNull.name = Name;
		NewNull.anchorPoint.setValue([
			NewNull.width / 2, NewNull.height / 2
		]);
		NewNull.enabled = false;
		NewNull.scale.setValue([10, 10]);
		var CellShapeLayer = CurComp.layers.addShape();
		CellShapeLayer.scale.setValue([90, 90]);
		CellShapeLayer.parent = NewNull;
		var CellShapeContents = CellShapeLayer
			.property("Contents")
			.addProperty("ADBE Vector Group")
			.property("Contents");

		var CellRect = CellShapeContents.addProperty("ADBE Vector Shape - Rect");
		CellRect.property("ADBE Vector Rect Size")
			.setValue([CellWidth, CellWidth]);
		CellRect.property("ADBE Vector Rect Roundness")
			.setValue(CellWidth / 10);

		CellShapeContents
			.addProperty("ADBE Vector Graphic - Fill")
			.property("ADBE Vector Fill Color")
			.setValue(Color);

		var CellStroke = CellShapeContents
			.addProperty("ADBE Vector Graphic - Stroke");
		CellStroke.property("ADBE Vector Stroke Color")
			.setValue([0, 0, 0]);
		CellStroke.property("ADBE Vector Stroke Width")
			.setValue(2);
		return NewNull;
	}

	function hslToRgb(h, s, l) {
		var r, g, b;
		if (s == 0) {
			r = g = b = l;
		}
		else {
			var hue2rgb = function hue2rgb(p, q, t) {
				if (t < 0) t += 1;
				if (t > 1) t -= 1;
				if (t < 1 / 6) return p + (q - p) * 6 * t;
				if (t < 1 / 2) return q;
				if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
				return p;
			}

			var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
			var p = 2 * l - q;
			r = hue2rgb(p, q, h + 1 / 3);
			g = hue2rgb(p, q, h);
			b = hue2rgb(p, q, h - 1 / 3);
		}

		return [r, g, b];
	}

	/// Generate
	var RegisterA = AddRegister("RegisterA");
	RegisterA.position.setValue([
		Size[0] / 2 - (RegisterCellCount * CellWidth) / 2.0 + CellWidth / 2,
		Size[1] * 1 / 8
	]);

	var RegisterB = AddRegister("RegisterB");
	RegisterB.position.setValue([
		Size[0] / 2 - (RegisterCellCount * CellWidth) / 2.0 + CellWidth / 2,
		Size[1] * 7 / 8
	]);

	var GridOff = [
		Size[0] / 2 - (CellColumns * CellWidth) / 2 + CellWidth / 2,
		Size[1] / 2 - (CellRows * CellWidth) / 2 + CellWidth / 2
	];

	var CellArray = [];
	for (var CurRow = 0; CurRow < CellRows; ++CurRow) {
		for (var CurCol = 0; CurCol < CellColumns; ++CurCol) {
			var CurNull = AddCell(
				"Cell[" + CurRow.toString() + ":" + CurCol.toString() + "]",
				hslToRgb(
					(CurRow * CellColumns + CurCol) / CellCount,
					1.0,
					0.5
				)
			);
			CurNull.position.setValueAtTime(
				0, [
					GridOff[0] + CurCol * CellWidth,
					GridOff[1] + CurRow * CellWidth
				]);
			CellArray.push(CurNull);
		}
	}

	var Index = 0;
	var Phase = 0;
	function SwapPhase(Alignment, OpName) {
		for (var ElemIdx = Math.floor(Index / Alignment);
			ElemIdx < Math.floor((CellCount / 2) / Alignment);
			++ElemIdx
		) {
			var PhaseIn = Phase * SwapDuration;
			var Marker = new MarkerValue(
				OpName + "-" + Alignment.toString() + " " + (Phase + 1).toString()
			);
			Marker.duration = SwapDuration;
			CurComp.markerProperty.setValueAtTime(PhaseIn, Marker);

			var LowerIdx = Index;
			var Lower = CellArray.slice(Index, Index + Alignment);
			var UpperIdx = CellCount - Index - Alignment;
			var Upper = CellArray.slice(
				CellCount - Index - Alignment,
				CellCount - Index
			);

			Log(OpName, PhaseIn + 1 / 6 * SwapDuration, 5 / 6 * SwapDuration);

			// Swap Lower
			Lower.map(function (CurCell, i) {

				var CellDelay = (1/12 * SwapDuration) * (i/Lower.length);
				// Move to register
				CurCell.position.setValueAtTime(
					PhaseIn,
					CurCell.position.value
				)
				CurCell.position.setValuesAtTimes(
					[
						PhaseIn + 1 / 6 * SwapDuration + CellDelay,
						PhaseIn + 2 / 6 * SwapDuration
					],
					[
						RegisterA.position.value
						+ [(i % Alignment) * CellWidth, 0],
						RegisterA.position.value
						+ [(i % Alignment) * CellWidth, 0]
					]
				);
				// Reverse in register
				CurCell.position.setValuesAtTimes(
					[
						PhaseIn + 3 / 6 * SwapDuration,
						PhaseIn + 4 / 6 * SwapDuration
					],
					[
						RegisterA.position.value
						+ [(Alignment - (i % Alignment) - 1) * CellWidth, 0],
						RegisterA.position.value
						+ [(Alignment - (i % Alignment) - 1) * CellWidth, 0]
					]
				);
				// Move to end of array
				CurCell.position.setValuesAtTimes(
					[
						PhaseIn + 5 / 6 * SwapDuration + CellDelay,
						PhaseIn + 6 / 6 * SwapDuration
					],
					[
						[
							GridOff[0]
							+ Math.floor((UpperIdx + (Alignment - i - 1)) % CellColumns)
							* CellWidth,
							GridOff[1]
							+ Math.floor((UpperIdx + (Alignment - i - 1)) / CellColumns)
							* CellWidth
						],
						[
							GridOff[0]
							+ Math.floor((UpperIdx + (Alignment - i - 1)) % CellColumns)
							* CellWidth,
							GridOff[1]
							+ Math.floor((UpperIdx + (Alignment - i - 1)) / CellColumns)
							* CellWidth
						]
					]
				);
				for (var i = 1; i <= CurCell.position.numKeys; ++i) {
					CurCell.position.setInterpolationTypeAtKey(i,
						KeyframeInterpolationType.BEZIER,
						KeyframeInterpolationType.BEZIER
					);
					CurCell.position.setTemporalEaseAtKey(
						i,
						[new KeyframeEase(1/6,100)]
					);
				}
			});
			// Swap Higher
			Upper.map(function (CurCell, i) {
				var CellDelay = (1/12 * SwapDuration) * (i/Lower.length);
				// Move to register
				CurCell.position.setValueAtTime(
					PhaseIn,
					CurCell.position.value
				)
				CurCell.position.setValuesAtTimes(
					[
						PhaseIn + 1 / 6 * SwapDuration + CellDelay,
						PhaseIn + 2 / 6 * SwapDuration,
					],
					[
						RegisterB.position.value
						+ [(i % Alignment) * CellWidth, 0],
						RegisterB.position.value
						+ [(i % Alignment) * CellWidth, 0]
					]
				);

				// Reverse in register
				CurCell.position.setValuesAtTimes(
					[
						PhaseIn + 3 / 6 * SwapDuration,
						PhaseIn + 4 / 6 * SwapDuration
					],
					[
						RegisterB.position.value
						+ [(Alignment - (i % Alignment) - 1) * CellWidth, 0],
						RegisterB.position.value
						+ [(Alignment - (i % Alignment) - 1) * CellWidth, 0]
					]
				);
				// Move to end of array
				CurCell.position.setValuesAtTimes(
					[
						PhaseIn + 5 / 6 * SwapDuration + CellDelay,
						PhaseIn + 6 / 6 * SwapDuration
					],
					[
						[
							GridOff[0]
							+ Math.floor((LowerIdx + (Alignment - i - 1)) % CellColumns)
							* CellWidth,
							GridOff[1]
							+ Math.floor((LowerIdx + (Alignment - i - 1)) / CellColumns)
							* CellWidth
						],
						[
							GridOff[0]
							+ Math.floor((LowerIdx + (Alignment - i - 1)) % CellColumns)
							* CellWidth,
							GridOff[1]
							+ Math.floor((LowerIdx + (Alignment - i - 1)) / CellColumns)
							* CellWidth
						]
					]
				);

				// Smooth keys
				for (var i = 1; i <= CurCell.position.numKeys; ++i) {
					CurCell.position.setInterpolationTypeAtKey(i,
						KeyframeInterpolationType.BEZIER,
						KeyframeInterpolationType.BEZIER
					);
					CurCell.position.setTemporalEaseAtKey(
						i,
						[new KeyframeEase(1/6,100)]
					);
				}
			});
			++Phase;
			Index += Alignment;
		}
	}

	Alignments.map(function (CurAlign, i) {
		SwapPhase(CurAlign, OpNames[i]);
	});
	CurComp.openInViewer();
}

GenSwap();