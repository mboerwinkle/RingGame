import json
def readManifest(path):
	manifest = None
	try:
		with open(path, mode="r") as fd:
			manifest = json.load(fd)
	except OSError as err:
		print("Could not open manifest at %s!" % path)
	except Exception as err:
		print("Error loading manifest:",err)
	return manifest
