import requests

def test_file_found(url):
    session = requests.Session()
    req = requests.Request('GET', url)
    prepared = req.prepare()

    prepared.url = url 

    response = session.send(prepared)
    return response.status_code == 200

server = "http://localhost:3000"

"""
    {
        "url": "//", # library gets rid of second '/'
        "expected_result": False,
    },   

    To Test, run: 

        curl localhost:3000// --path-as-is

    Example:

        $ curl localhost:3000// --path-as-is
        {"error": "Failed to handle request","success": false}
        
    Server output should look like this:

        [1894]: PATH //
        [1894]: Failed to open file
        [1894]: Failed to handle GET request
        [1894]: Elapsed: 91519 ns (0.092 ms)
"""

check_exists = [
    {
        "url": "/",
        "expected_result": True,
    },
    {
        "url": "/index.html",
        "expected_result": True,
    },
    {
        "url": "/index.html.png",
        "expected_result": False,
    },
    {
        "url": "/%69ndex.html",
        "expected_result": True,
    },
    {
        "url": "/%20ndex.html",
        "expected_result": False
    },
    {
        "url": "",
        "expected_result": True,
    },
    {
        "url": "/../",
        "expected_result": False,
    },
    {
        "url": "//./../index.html",
        "expected_result": False,
    },
    {
        "url": "/.//index.html",
        "expected_result": False,
    },
]

succeeded = 0
total = len(check_exists)

for i in range(len(check_exists)):
    check_exists[i]["result"] = test_file_found(server + check_exists[i]["url"])
    if (check_exists[i]["result"] == check_exists[i]["expected_result"]):
        succeeded += 1
    else:
        print(f"Test #{i + 1} failed")

print(f"{(succeeded / total) * 100}% of tests passed")