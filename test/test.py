import requests

def test_file_found(url):
    session = requests.Session()
    req = requests.Request('GET', url)
    prepared = req.prepare()

    prepared.url = url 

    response = session.send(prepared)
    return response.status_code == 200

server = "http://localhost:3000"

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
        "url": "",
        "expected_result": True,
    },
    {
        "url": "/../",
        "expected_result": False,
    },
    {
        "url": "//", # should be failing with sanitize_path,
                     # but perhaps the requests library is making 
                     # the '//' -> '/' before the request is sent,
                     # since on the server I see "HTTPREQ PATH: /" 
                     # with logging
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