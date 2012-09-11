<?php

if (extension_loaded ("secret_blame")) {
	echo "secret_blame is loaded!\n";
} else {
	echo "secret_blame is not loaded\n";
}

non_existent_function ();
