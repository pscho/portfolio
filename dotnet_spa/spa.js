var submitInProgress = false;

function fillMainTable() {
	if (this.readyState == 4) {
		if (this.status != 200) {
			console.log("error - fillMainTable");
			console.log(this);
			document.getElementById("main_table").innerHTML = 
				"Error retrieving data. Please refresh the page and try again.";
			return;
		}

		let rowsStr = "<tbody>\r\n<tr>\r\n";
		let colNames = this.response["colNames"]
		for (let i = 0; i < colNames.length; ++i) {
			rowsStr += "\t<th>" + colNames[i] + "</th>\r\n";
		}
		rowsStr += "</tr>\r\n";
		
		let rowStr = "";
		let rows = this.response["rows"];
		for (let i = 0; i < rows.length; ++i) {
			rowStr = "<tr>\r\n";
			for (let j = 0; j < colNames.length; ++j) {
				rowStr += "\t<td>" + rows[i][j] + "</td>\r\n";
			}
			rowStr += "</tr>\r\n";
			rowsStr += rowStr; 
		}
		rowsStr += "</tbody>\r\n";
		
		document.getElementById("main_table").innerHTML = rowsStr;
	}
}

function closeForm(form) {
	return function() {
		form.remove();
	}
}

function submitForm(event) {
	event.preventDefault();

	if (!submitInProgress) {
		submitInProgress = true;

		let allFormInputs = document.getElementsByClassName("form_input");
		for (let i = 0; i < allFormInputs.length; ++i) {
			allFormInputs[i].setAttribute("disabled", null);
		}
		
		let allFormSubmits = document.getElementsByClassName("form_submit");
		for (let i = 0; i < allFormSubmits.length; ++i) {
			allFormSubmits[i].setAttribute("disabled", null);
		}

		let form = event.target;
		let formData = {};
		for (let i = 0; i < form.children.length; ++i) {
			let child = form.children[i];
			for (let j = 0; j < child.classList.length; ++j) {
				if ("form_input" == child.classList[j]) {
					formData[child.name] = child.value;
				}
			}
		}

		// xmlhttprequest submit
		var submitReq = new XMLHttpRequest();
		submitReq.responseType = "json";
		submitReq.onreadystatechange = processSubmit(event.target);
		submitReq.open("POST", "/test");
		//submitReq.send(formData);
	}
}

function processSubmit(form) {
	return function() {
		if (this.readState == 4) {
			let allFormInputs = document.getElementsByClassName("form_input");
			for (let i = 0; i < allFormInputs.length; ++i) {
				allFormInputs[i].removeAttribute("disabled");
			}
			
			let allFormSubmits = document.getElementsByClassName("form_submit");
			for (let i = 0; i < allFormSubmits.length; ++i) {
				allFormSubmits[i].removeAttribute("disabled");
			}

			if (this.status == 422) {  // Validation error


			} else if (this.status == 201) { // Success
				// Refresh main table
				let fillMainTableReq = new XMLHttpRequest();
				fillMainTableReq.responseType = "json";
				//fillMainTableReq.addEventListener("load", fillMainTableListener);
				fillMainTableReq.onreadystatechange = fillMainTable;
				fillMainTableReq.open("GET", "/customers");
				fillMainTableReq.send();

				for (let i = 0; i < form.children.length; ++i) {
					form.children[i].remove();
				}

				let pElem = document.createElement("p");
				pElem.appendChild(document.createTextNode("Submission successful."));
				form.children.appendChild(pElem);

				let closeBtn = document.createElement("button");
				closeBtn.appendChild(document.createTextNode("Close"));
				closeBtn.addEventListener("click", closeForm(form));
				form.children.appendChild(closeBtn);

			} else {
				console.log("error - processSubmit " + form);
				console.log(this);

				alert("Error on form submission. Please try again.");
			}

			submitInProgress = false;
		}
	}
}

function domContentLoaded() {
	let fillMainTableReq = new XMLHttpRequest();
	fillMainTableReq.responseType = "json";
	//fillMainTableReq.addEventListener("load", fillMainTableListener);
	fillMainTableReq.onreadystatechange = fillMainTable;
	fillMainTableReq.open("GET", "/customers");
	fillMainTableReq.send();

	let forms = document.getElementsByTagName("form");
	for (let i = 0; i < forms.length; ++i) {
		forms[i].addEventListener("submit", submitForm);
	}	
}
document.addEventListener("DOMContentLoaded", domContentLoaded);

