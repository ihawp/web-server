class Manager {
    constructor(url, amount, drawer, start, stop) {
        this.url = url;
        this.amount = amount;
        this.drawer = drawer;
        this.running = 0;
        this.i = 0;
        this.total = 0;
        start.addEventListener('click', this.start.bind(this));
        stop.addEventListener('click', this.stopRunning.bind(this));
        reset.addEventListener('click', this.reset.bind(this));
        this.output = document.getElementById('output');

        this.totalTime = 0;
        this.startTime = 0;
        this.endTime = 0;
    }

    start = (event) => {
        if (this.running) return;
        // reset the counter for printing time and amount of requests
        if (this.i > 0) {
            this.total += this.i;
            this.i = 0;
        }
        this.running = 1;
        this.startTime = Date.now();
        this.run();
    }

    stopRunning = () => this.running = 0;

    stop = () => {
        this.stopRunning();
        this.endTime = Date.now();
        this.totalTime += this.endTime - this.startTime;
        let theString = `Total Time (${this.i + this.total}): ${this.totalTime}`;
        this.output.innerText = theString;
        console.log(theString);
    }

    reset = () => {
        this.stop();
        this.i = 0;
        this.totalTime = 0;
        this.total = 0;
        this.output.innerText = "";
        this.drawer.reset();
    }

    async makeFetch(url = "") {
        try {
            const response = await fetch(window.location.href + url, {
                method: 'GET',
                headers: {
                    'Content-Type': 'text/html'
                }
            });
            if (!response.ok)  return 0;
            const data = await response.json();
            if (!data.success) return 0;
            return 1;
        } catch (error) {
            return 0;
        }
    }

    async run() {
        while (this.running && this.i + this.total < this.amount) {
            this.drawer.draw(await this.makeFetch(this.url));
            this.i++;
        }
        this.stop();
    }

}

class Drawer {
    constructor(canvas, amount) {
        this.canvas = canvas;
        this.context = canvas.getContext("2d");
        this.responses = []; // track responses?

        this.ww = window.innerWidth;
        this.wh = window.innerHeight;
        this.canvas.width = this.ww;
        this.canvas.height = this.wh;

        this.columns = amount / 10;
        this.rows = amount / this.columns;

        this.width = this.ww / this.columns;
        this.height = this.wh / this.rows;
        this.drawX = 0;
        this.drawY = 0;
    }

    draw = (item) => {
        this.responses.push(item);

        if (this.drawX + this.width > window.innerWidth) {
            this.drawX = 0;
            this.drawY += this.height;
        }

        if (item) {
            this.context.fillStyle = 'green';
        } else {
            this.context.fillStyle = 'red';
        }

        this.context.fillRect(this.drawX, this.drawY, this.width, this.height);
        this.drawX += this.width;
    }

    printResponses = () => {
        this.responses.forEach(item => this.draw(item));
    }

    clearCanvas = () => {
        this.context.clearRect(0, 0, this.canvas.width, this.canvas.height)
    }

    reset = () => {
        this.drawX = 0;
        this.drawY = 0;
        this.clearCanvas();
    }
}

let amount = 500;
new Manager("error-response", amount, new Drawer(canvas, amount), start, stopp);