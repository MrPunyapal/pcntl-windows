<?php
declare(strict_types=1);
exit(getmypid() === (int)($argv[1] ?? -1) ? 73 : 74);
